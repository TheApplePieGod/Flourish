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

#ifdef FL_PLATFORM_WINDOWS
    #include "FlourishTesting/WindowsWindow.h"
#elif defined(FL_PLATFORM_MACOS)
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
            HWND window = Windows::CreateWindowAndGet((int)contextCreateInfo.Width, (int)contextCreateInfo.Height);
            contextCreateInfo.Instance = instance;
            contextCreateInfo.Window = window;
        #elif defined(FL_PLATFORM_MACOS)
            void* view = MacOS::CreateWindowAndGetView((int)contextCreateInfo.Width, (int)contextCreateInfo.Height);
            contextCreateInfo.NSView = view;
        #endif
        auto renderContext = Flourish::RenderContext::Create(contextCreateInfo);
        
        struct Vertex
        {
            float Position[3];
            float TexCoord[2];
        };

        Flourish::BufferCreateInfo bufCreateInfo;
        bufCreateInfo.Type = Flourish::BufferType::Vertex;
        bufCreateInfo.Usage = Flourish::BufferUsageType::Dynamic;
        bufCreateInfo.Layout = { { Flourish::BufferDataType::Float3 }, { Flourish::BufferDataType::Float2 } };
        bufCreateInfo.ElementCount = 3;
        auto buffer = Flourish::Buffer::Create(bufCreateInfo);

        bufCreateInfo.Type = Flourish::BufferType::Storage;
        bufCreateInfo.Usage = Flourish::BufferUsageType::Dynamic;
        bufCreateInfo.Layout = { { Flourish::BufferDataType::Float4 } };
        bufCreateInfo.ElementCount = 2;
        auto uniform = Flourish::Buffer::Create(bufCreateInfo);

        Flourish::RenderPassCreateInfo rpCreateInfo;
        rpCreateInfo.ColorAttachments = {
            { Flourish::ColorFormat::RGBA8_UNORM }
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
            layout(location = 1) in vec2 inTexCoord;

            layout(location = 0) out vec2 outTexCoord;

            void main() {
                gl_Position = vec4(inPosition, 1.f);
                outTexCoord = inTexCoord;
            }
        )";
        auto vertShader = Flourish::Shader::Create(vsCreateInfo);

        Flourish::ShaderCreateInfo fsCreateInfo;
        fsCreateInfo.Type = Flourish::ShaderType::Fragment;
        fsCreateInfo.Source = R"(
            #version 460

            layout(location = 0) in vec2 inTexCoord;

            layout(location = 0) out vec4 outColor;

            layout(binding = 0) readonly buffer ColorBuffer {
                vec4 color;
            } colorBuffer;

            layout(binding = 1) uniform sampler2D tex;

            void main() {
                // outColor = vec4(1.f, 0.f, 0.f, 1.f);
                // outColor = vec4(colorBuffer.color.rgb, 1.f);
                outColor = texture(tex, inTexCoord);
            }
        )";
        auto fragShader = Flourish::Shader::Create(fsCreateInfo);

        Flourish::GraphicsPipelineCreateInfo gpCreateInfo;
        gpCreateInfo.VertexShader = vertShader;
        gpCreateInfo.FragmentShader = fragShader;
        gpCreateInfo.VertexInput = true;
        gpCreateInfo.VertexTopology = Flourish::VertexTopology::TriangleList;
        gpCreateInfo.VertexLayout = buffer->GetLayout();
        gpCreateInfo.BlendStates = { { false } };
        gpCreateInfo.DepthTest = true;
        gpCreateInfo.DepthWrite = true;
        gpCreateInfo.CullMode = Flourish::CullMode::Backface;
        gpCreateInfo.WindingOrder = Flourish::WindingOrder::Clockwise;
        renderPass->CreatePipeline("main", gpCreateInfo);
        renderContext->GetRenderPass()->CreatePipeline("main", gpCreateInfo);

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

        texCreateInfo.Width = contextCreateInfo.Width;
        texCreateInfo.Height = contextCreateInfo.Height;
        texCreateInfo.Channels = 4;
        texCreateInfo.DataType = Flourish::BufferDataType::UInt8;
        texCreateInfo.UsageType = Flourish::BufferUsageType::Dynamic;
        texCreateInfo.SamplerState.AnisotropyEnable = false;
        texCreateInfo.InitialData = nullptr;
        auto frameTex = Flourish::Texture::Create(texCreateInfo);

        Flourish::FramebufferCreateInfo fbCreateInfo;
        fbCreateInfo.RenderPass = renderPass;
        fbCreateInfo.ColorAttachments = { { { 0.f, 0.f, 0.f, 0.f }, frameTex } };
        fbCreateInfo.DepthAttachments = { {} };
        fbCreateInfo.Width = 1920;
        fbCreateInfo.Height = 1080;
        auto framebuffer = Flourish::Framebuffer::Create(fbCreateInfo);

        Flourish::CommandBufferCreateInfo cmdCreateInfo;
        auto cmdBuffer = Flourish::CommandBuffer::Create(cmdCreateInfo);

        while (true)
        {
            Flourish::Context::BeginFrame();

            #ifdef FL_PLATFORM_WINDOWS
                Windows::PollEvents(window);
            #elif defined(FL_PLATFORM_MACOS)
                MacOS::PollEvents();
            #endif

            Vertex vertices[3] = {
                { { 0.f, 0.f, 0.f }, { 0.f, 0.f } },
                { { 0.5f, 0.f, 0.f }, { 1.f, 0.f } },
                { { 0.f, 0.5f, 0.f }, { 0.f, 1.f } }
            };
            buffer->SetBytes(vertices, sizeof(vertices), 0);
            buffer->Flush();

            float color[8] = { 0.f, 1.f, 0.f, 1.f, 1.f, 0.f, 1.f, 1.f };
            uniform->SetBytes(color, sizeof(color), 0);

            if (texture->IsReady())
            {
                /*
                auto encoder1 = cmdBuffer->EncodeRenderCommands(framebuffer.get());
                encoder1->BindPipeline("main");
                encoder1->BindPipelineBufferResource(0, uniform.get(), 0, 0, 3);
                encoder1->BindPipelineTextureResource(1, texture.get());
                encoder1->BindVertexBuffer(buffer.get()); // TODO: validate buffer is actually a vertex
                encoder1->Draw(3, 0, 1);
                encoder1->EndEncoding();
                */

                auto frameEncoder = renderContext->EncodeFrameRenderCommands();
                frameEncoder->BindPipeline("main");
                frameEncoder->BindPipelineBufferResource(0, uniform.get(), 0, 0, 1);
                frameEncoder->BindPipelineTextureResource(1, texture.get());
                frameEncoder->FlushPipelineBindings();
                frameEncoder->BindVertexBuffer(buffer.get()); // TODO: validate buffer is actually a vertex
                frameEncoder->Draw(3, 0, 1);
                frameEncoder->EndEncoding();

                renderContext->Present({ { cmdBuffer.get() } });
            }
            
            Flourish::Context::EndFrame();
        }
    }
    
    Flourish::Context::Shutdown();
    return 0;
}