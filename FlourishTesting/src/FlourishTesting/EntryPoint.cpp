#include "flpch.h"

#include "Flourish/Api/Context.h"
#include "Flourish/Api/RenderContext.h"
#include "Flourish/Api/CommandBuffer.h"
#include "Flourish/Api/Buffer.h"
#include "Flourish/Api/RenderPass.h"
#include "Flourish/Api/Texture.h"
#include "Flourish/Api/Framebuffer.h"
#include "Flourish/Api/RenderCommandEncoder.h"

#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "stb_image/stb_image.h"

#ifdef FL_PLATFORM_MACOS
    #include "FlourishTesting/MacWindow.h"
#endif

std::shared_ptr<spdlog::logger> logger; 

void Log(Flourish::LogLevel level, const char* message)
{
    logger->log((spdlog::level::level_enum)level, message);
}

int main(int argc, char** argv)
{
    // Initialize logging
    auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    consoleSink->set_pattern("%^[%T] %n: %v%$");
    logger = std::make_shared<spdlog::logger>("FLOURISH", consoleSink);
    logger->set_level(spdlog::level::trace);
    logger->flush_on(spdlog::level::trace);
    spdlog::register_logger(logger);
    Flourish::Logger::SetLogFunction(Log);

    Flourish::ContextInitializeInfo contextInitInfo;
    contextInitInfo.Backend = Flourish::BackendType::Vulkan;
    contextInitInfo.ApplicationName = "FlourishTesting";
    Flourish::Context::Initialize(contextInitInfo);

    {
        Flourish::RenderContextCreateInfo contextCreateInfo;
        contextCreateInfo.Width = 1920;
        contextCreateInfo.Height = 1080;
        #ifdef FL_PLATFORM_WINDOWS
            HINSTANCE instance = GetModuleHandle(NULL);
            WNDCLASS wc{};
            wc.lpfnWndProc = DefWindowProc;
            wc.hInstance = instance;
            wc.lpszClassName = "Window";
            RegisterClass(&wc);
            HWND hwnd = CreateWindow(
                "Window",
                "Flourish",
                WS_OVERLAPPEDWINDOW,
                CW_USEDEFAULT, CW_USEDEFAULT,
                (int)contextCreateInfo.Width, (int)contextCreateInfo.Height,
                NULL,
                NULL,
                instance,
                NULL
            );
            ShowWindow(hwnd, SW_SHOW);

            contextCreateInfo.Instance = instance;
            contextCreateInfo.Window = hwnd;
        #elif defined(FL_PLATFORM_MACOS)
            void* view = MacOS::CreateWindowAndGetView();
            contextCreateInfo.NSView = view;
        #endif
        auto renderContext = Flourish::RenderContext::Create(contextCreateInfo);

        Flourish::BufferCreateInfo bufCreateInfo;
        bufCreateInfo.Type = Flourish::BufferType::Uniform;
        bufCreateInfo.Usage = Flourish::BufferUsageType::Dynamic;
        bufCreateInfo.Layout = { { Flourish::BufferDataType::Float4 } };
        bufCreateInfo.ElementCount = 1;
        auto buffer = Flourish::Buffer::Create(bufCreateInfo);

        Flourish::RenderPassCreateInfo rpCreateInfo;
        rpCreateInfo.ColorAttachments = {
            { Flourish::ColorFormat::RGBA8_SRGB }
        };
        rpCreateInfo.DepthAttachments = { {} };
        rpCreateInfo.Subpasses = {
            { {}, { { Flourish::SubpassAttachmentType::Color, 0 } } }
        };
        rpCreateInfo.SampleCount = Flourish::MsaaSampleCount::Four;
        auto renderPass = Flourish::RenderPass::Create(rpCreateInfo);

        Flourish::ShaderCreateInfo vsCreateInfo;
        vsCreateInfo.Type = Flourish::ShaderType::Vertex;
        vsCreateInfo.Source = R"(
            #version 460

            layout(location = 0) in vec3 inPosition;

            void main() {
                gl_Position = vec4(inPosition, 1.f);
            }
        )";
        auto vertShader = Flourish::Shader::Create(vsCreateInfo);

        Flourish::ShaderCreateInfo fsCreateInfo;
        fsCreateInfo.Type = Flourish::ShaderType::Fragment;
        fsCreateInfo.Source = R"(
            #version 460

            layout(location = 0) out vec4 outColor;

            void main() {
                outColor = vec4(1.f);
            }
        )";
        auto fragShader = Flourish::Shader::Create(fsCreateInfo);

        Flourish::GraphicsPipelineCreateInfo gpCreateInfo;
        gpCreateInfo.VertexShader = vertShader;
        gpCreateInfo.FragmentShader = fragShader;
        gpCreateInfo.VertexInput = true;
        gpCreateInfo.VertexTopology = Flourish::VertexTopology::TriangleList;
        gpCreateInfo.VertexLayout = { { Flourish::BufferDataType::Float3 } };
        gpCreateInfo.BlendStates = { { false } };
        gpCreateInfo.DepthTest = true;
        gpCreateInfo.DepthWrite = true;
        gpCreateInfo.CullMode = Flourish::CullMode::Backface;
        gpCreateInfo.WindingOrder = Flourish::WindingOrder::Clockwise;
        renderPass->CreatePipeline("main", gpCreateInfo);

        int imageWidth;
        int imageHeight;
        int imageChannels;
        unsigned char* imagePixels = stbi_load("resources/image.png", &imageWidth, &imageHeight, &imageChannels, 4);
        Flourish::TextureCreateInfo texCreateInfo;
        texCreateInfo.Width = static_cast<u32>(imageWidth);
        texCreateInfo.Height = static_cast<u32>(imageHeight);
        texCreateInfo.Channels = 4;
        texCreateInfo.DataType = Flourish::BufferDataType::UInt8;
        texCreateInfo.UsageType = Flourish::BufferUsageType::Static;
        texCreateInfo.InitialData = imagePixels;
        texCreateInfo.InitialDataSize = imageWidth * imageHeight * 4;
        texCreateInfo.SamplerState.AnisotropyEnable = false;
        auto texture = Flourish::Texture::Create(texCreateInfo);
        delete[] imagePixels;

        Flourish::FramebufferCreateInfo fbCreateInfo;
        fbCreateInfo.RenderPass = renderPass;
        fbCreateInfo.ColorAttachments = { {} };
        fbCreateInfo.DepthAttachments = { {} };
        fbCreateInfo.Width = 1920;
        fbCreateInfo.Height = 1080;
        auto framebuffer = Flourish::Framebuffer::Create(fbCreateInfo);

        float val = 3.f;
        buffer->SetBytes(&val, sizeof(float), 0);
        buffer->Flush();

        Flourish::CommandBufferCreateInfo cmdCreateInfo;
        auto cmdBuffer = Flourish::CommandBuffer::Create(cmdCreateInfo);

        auto frameEncoder = renderContext->EncodeFrameRenderCommands();
        frameEncoder->EndEncoding();
    }
    
    Flourish::Context::Shutdown();
    return 0;
}