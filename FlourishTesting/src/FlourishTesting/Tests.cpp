#include "flpch.h"
#include "Tests.h"

#ifdef FL_PLATFORM_WINDOWS
    #include "FlourishTesting/WindowsWindow.h"
#elif defined(FL_PLATFORM_MACOS)
    #include "FlourishTesting/MacWindow.h"
#endif

#include "stb_image/stb_image.h"

namespace FlourishTesting
{
    #ifdef FL_PLATFORM_WINDOWS
    HWND window;
    #endif

    void Tests::Run()
    {
        while (true)
        {
            Flourish::Context::BeginFrame();

            #ifdef FL_PLATFORM_WINDOWS
                Windows::PollEvents(window);
            #elif defined(FL_PLATFORM_MACOS)
                MacOS::PollEvents();
            #endif

            RunSingleThreadedTest();

            Flourish::Context::EndFrame();
        }
    }
    
    void Tests::RunSingleThreadedTest()
    {
        if (!m_DogTexture->IsReady()) return;

        auto encoder1 = m_CommandBuffers[0]->EncodeRenderCommands(m_SimplePassNoDepthFrameTexFB.get());
        encoder1->BindPipeline("image");
        encoder1->BindPipelineTextureResource(0, m_DogTexture.get());
        encoder1->FlushPipelineBindings();
        encoder1->BindVertexBuffer(m_QuadVertices.get()); // TODO: validate buffer is actually a vertex
        encoder1->BindIndexBuffer(m_QuadIndices.get()); // TODO: validate buffer is actually a vertex
        encoder1->DrawIndexed(m_QuadIndices->GetAllocatedCount(), 0, 0, 1);
        encoder1->EndEncoding();

        auto frameEncoder = m_RenderContext->EncodeFrameRenderCommands();
        frameEncoder->BindPipeline("main");
        frameEncoder->BindPipelineTextureResource(0, m_UIntFrameTex.get());
        frameEncoder->FlushPipelineBindings();
        frameEncoder->BindVertexBuffer(m_FullTriangleVertices.get()); // TODO: validate buffer is actually a vertex
        frameEncoder->Draw(3, 0, 1);
        frameEncoder->EndEncoding();

        m_RenderContext->Present({ { m_CommandBuffers[0].get() } });
    }

    Tests::Tests()
    {
        Flourish::RenderContextCreateInfo contextCreateInfo;
        contextCreateInfo.Width = m_ScreenWidth;
        contextCreateInfo.Height = m_ScreenHeight;
        #ifdef FL_PLATFORM_WINDOWS
            HINSTANCE instance = GetModuleHandle(NULL);
            window = Windows::CreateWindowAndGet((int)contextCreateInfo.Width, (int)contextCreateInfo.Height);
            contextCreateInfo.Instance = instance;
            contextCreateInfo.Window = window;
        #elif defined(FL_PLATFORM_MACOS)
            void* view = MacOS::CreateWindowAndGetView((int)contextCreateInfo.Width, (int)contextCreateInfo.Height);
            contextCreateInfo.NSView = view;
        #endif
        m_RenderContext = Flourish::RenderContext::Create(contextCreateInfo);
        
        CreateRenderPasses();
        CreatePipelines();
        CreateBuffers();
        CreateTextures();
        CreateFramebuffers();
        CreateCommandBuffers();
    }    
    
    void Tests::CreateRenderPasses()
    {
        Flourish::RenderPassCreateInfo rpCreateInfo;
        
        // Simple pass no depth
        rpCreateInfo.ColorAttachments = {
            { Flourish::ColorFormat::RGBA8_UNORM }
        };
        rpCreateInfo.DepthAttachments = { {} };
        rpCreateInfo.Subpasses = {
            { {}, { { Flourish::SubpassAttachmentType::Color, 0 } } }
        };
        rpCreateInfo.SampleCount = Flourish::MsaaSampleCount::None;
        m_SimplePassNoDepth = Flourish::RenderPass::Create(rpCreateInfo);
    }

    void Tests::CreatePipelines()
    {
        Flourish::ShaderCreateInfo vsCreateInfo;
        Flourish::ShaderCreateInfo fsCreateInfo;
        Flourish::GraphicsPipelineCreateInfo gpCreateInfo;

        // Simple vert shader
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
        auto simpleVertShader = Flourish::Shader::Create(vsCreateInfo);

        // Image frag shader
        fsCreateInfo.Type = Flourish::ShaderType::Fragment;
        fsCreateInfo.Source = R"(
            #version 460

            layout(location = 0) in vec2 inTexCoord;

            layout(location = 0) out vec4 outColor;

            layout(binding = 0) uniform sampler2D tex;

            void main() {
                outColor = texture(tex, inTexCoord);
            }
        )";
        auto imageFragShader = Flourish::Shader::Create(fsCreateInfo);

        // Render context primary pipeline
        gpCreateInfo.VertexShader = simpleVertShader;
        gpCreateInfo.FragmentShader = imageFragShader;
        gpCreateInfo.VertexInput = true;
        gpCreateInfo.VertexTopology = Flourish::VertexTopology::TriangleList;
        gpCreateInfo.VertexLayout = m_VertexLayout;
        gpCreateInfo.BlendStates = { { false } };
        gpCreateInfo.DepthTest = true;
        gpCreateInfo.DepthWrite = true;
        gpCreateInfo.CullMode = Flourish::CullMode::Backface;
        gpCreateInfo.WindingOrder = Flourish::WindingOrder::Clockwise;
        m_RenderContext->GetRenderPass()->CreatePipeline("main", gpCreateInfo);

        m_SimplePassNoDepth->CreatePipeline("image", gpCreateInfo);
    }
    
    void Tests::CreateBuffers()
    {
        Flourish::BufferCreateInfo bufCreateInfo;

        // Full triangle vertices
        {
            Vertex vertices[3] = {
                { { -1.f, -1.f, 0.f }, { 0.f, 0.f } },
                { { 3.f, -1.f, 0.f }, { 2.f, 0.f } },
                { { -1.f, 3.f, 0.f }, { 0.f, 2.f } }
            };
            bufCreateInfo.Type = Flourish::BufferType::Vertex;
            bufCreateInfo.Usage = Flourish::BufferUsageType::Static;
            bufCreateInfo.Layout = m_VertexLayout;
            bufCreateInfo.ElementCount = 3;
            bufCreateInfo.InitialData = vertices;
            bufCreateInfo.InitialDataSize = sizeof(vertices);
            m_FullTriangleVertices = Flourish::Buffer::Create(bufCreateInfo);
        }
        
        // Quad vertices & indices
        {
            Vertex vertices[4] = {
                { { -0.5f, -0.5f, 0.f }, { 0.f, 0.f } },
                { { 0.5f, -0.5f, 0.f }, { 1.f, 0.f } },
                { { 0.5f, 0.5f, 0.f }, { 1.f, 1.f } },
                { { -0.5f, 0.5f, 0.f }, { 0.f, 1.f } }
            };
            u32 indices[6] = {
                0, 1, 3, 1, 2, 3
            };
            bufCreateInfo.Type = Flourish::BufferType::Vertex;
            bufCreateInfo.Usage = Flourish::BufferUsageType::Static;
            bufCreateInfo.Layout = m_VertexLayout;
            bufCreateInfo.ElementCount = 4;
            bufCreateInfo.InitialData = vertices;
            bufCreateInfo.InitialDataSize = sizeof(vertices);
            m_QuadVertices = Flourish::Buffer::Create(bufCreateInfo);

            bufCreateInfo.Type = Flourish::BufferType::Index;
            bufCreateInfo.Usage = Flourish::BufferUsageType::Static;
            bufCreateInfo.Layout = { { Flourish::BufferDataType::UInt } };
            bufCreateInfo.ElementCount = 6;
            bufCreateInfo.InitialData = indices;
            bufCreateInfo.InitialDataSize = sizeof(indices);
            m_QuadIndices = Flourish::Buffer::Create(bufCreateInfo);
        }
    }
    
    void Tests::CreateTextures()
    {
        Flourish::TextureCreateInfo texCreateInfo;

        // Dog image
        int imageWidth;
        int imageHeight;
        int imageChannels;
        unsigned char* imagePixels = stbi_load("resources/image.png", &imageWidth, &imageHeight, &imageChannels, 4);
        if (!imagePixels) { FL_LOG_WARN("Dog image failed to load"); }
        texCreateInfo.Width = static_cast<u32>(imageWidth);
        texCreateInfo.Height = static_cast<u32>(imageHeight);
        texCreateInfo.Channels = 4;
        texCreateInfo.DataType = Flourish::BufferDataType::UInt8;
        texCreateInfo.UsageType = Flourish::BufferUsageType::Static;
        if (imagePixels)
        {
            texCreateInfo.InitialData = imagePixels;
            texCreateInfo.InitialDataSize = imageWidth * imageHeight * 4;
        }
        texCreateInfo.SamplerState.AnisotropyEnable = false;
        m_DogTexture = Flourish::Texture::Create(texCreateInfo);
        delete[] imagePixels;

        texCreateInfo.Width = m_ScreenWidth;
        texCreateInfo.Height = m_ScreenHeight;
        texCreateInfo.Channels = 4;
        texCreateInfo.DataType = Flourish::BufferDataType::UInt8;
        texCreateInfo.UsageType = Flourish::BufferUsageType::Dynamic;
        texCreateInfo.SamplerState.AnisotropyEnable = false;
        texCreateInfo.InitialData = nullptr;
        m_UIntFrameTex = Flourish::Texture::Create(texCreateInfo);
    }
    
    void Tests::CreateFramebuffers()
    {
        Flourish::FramebufferCreateInfo fbCreateInfo;
        fbCreateInfo.RenderPass = m_SimplePassNoDepth;
        fbCreateInfo.ColorAttachments = { { { 0.f, 0.f, 0.f, 0.f }, m_UIntFrameTex } };
        fbCreateInfo.DepthAttachments = { {} };
        fbCreateInfo.Width = m_ScreenWidth;
        fbCreateInfo.Height = m_ScreenHeight;
        m_SimplePassNoDepthFrameTexFB = Flourish::Framebuffer::Create(fbCreateInfo);
    }
    
    void Tests::CreateCommandBuffers()
    {
        Flourish::CommandBufferCreateInfo cmdCreateInfo;
        
        for (u32 i = 0; i < 10; i++)
            m_CommandBuffers.push_back(Flourish::CommandBuffer::Create(cmdCreateInfo));
    }
}