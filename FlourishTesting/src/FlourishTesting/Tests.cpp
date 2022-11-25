#include "flpch.h"
#include "Tests.h"

#include "Flourish/Api/RenderCommandEncoder.h"
#include "Flourish/Api/ComputeCommandEncoder.h"

#include <future>

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

            if (m_RenderContext->Validate())
            {
                // RunSingleThreadedTest();
                RunMultiThreadedTest();
            }

            Flourish::Context::EndFrame();
        }
    }

    void Tests::RunMultiThreadedTest()
    {
        if (!m_DogTexture->IsReady()) return;
        if (!m_CatTexture->IsReady()) return;

        u32 objectCount = 5;

        std::vector<std::future<void>> jobs;
        std::vector<Flourish::CommandBuffer*> parallelBuffers;
        for (u32 i = 0; i < objectCount; i++)
        {
            jobs.push_back(std::async(std::launch::async, [&, i]()
            {
                auto encoder = m_CommandBuffers[i]->EncodeRenderCommands(m_FrameTextureBuffers[i].get());
                encoder->BindPipeline("simple_image");
                if (Flourish::Context::FrameCount() % 100 < 50)
                    encoder->BindPipelineTextureResource(0, m_DogTexture.get());
                else
                    encoder->BindPipelineTextureResource(0, m_CatTexture.get());
                encoder->FlushPipelineBindings();
                encoder->BindVertexBuffer(m_FullTriangleVertices.get()); 
                encoder->Draw(3, 0, 1);
                encoder->EndEncoding();
            }));
            
            parallelBuffers.push_back(m_CommandBuffers[i].get());
        }
        
        {
            jobs.push_back(std::async(std::launch::async, [&]()
            {
                auto encoder = m_CommandBuffers[objectCount]->EncodeComputeCommands(m_ComputeTarget.get());
                encoder->BindPipeline(m_ComputePipeline.get());
                encoder->BindPipelineBufferResource(0, m_ObjectData.get(), 0, 0, objectCount);
                encoder->FlushPipelineBindings();
                encoder->Dispatch(objectCount, 1, 1);
                encoder->EndEncoding();
            }));
            
            parallelBuffers.push_back(m_CommandBuffers[objectCount].get());
        }
        
        auto encoder = m_CommandBuffers[objectCount + 1]->EncodeRenderCommands(m_FrameTextureBuffers[objectCount].get());
        encoder->BindPipeline("object_image");
        for (u32 i = 0; i < objectCount; i++)
        {
            encoder->BindPipelineTextureResource(0, m_FrameTextures[i].get());
            encoder->BindPipelineBufferResource(1, m_ObjectData.get(), 0, i, 1);
            encoder->FlushPipelineBindings();
            encoder->BindVertexBuffer(m_QuadVertices.get()); 
            encoder->BindIndexBuffer(m_QuadIndices.get());
            encoder->DrawIndexed(m_QuadIndices->GetAllocatedCount(), 0, 0, 1);
        }
        encoder->EndEncoding();

        auto frameEncoder = m_RenderContext->EncodeFrameRenderCommands();
        frameEncoder->BindPipeline("main");
        frameEncoder->BindPipelineTextureResource(0, m_FrameTextures[objectCount].get());
        frameEncoder->FlushPipelineBindings();
        frameEncoder->BindVertexBuffer(m_FullTriangleVertices.get()); // TODO: validate buffer is actually a vertex
        frameEncoder->Draw(3, 0, 1);
        frameEncoder->EndEncoding();
        
        for (auto& job : jobs)
            job.wait();

        //Flourish::Context::SubmitCommandBuffers({{ m_CommandBuffers[objectCount].get() }});
        m_RenderContext->Present({ parallelBuffers, { m_CommandBuffers[objectCount + 1].get() } });
    }
    
    void Tests::RunSingleThreadedTest()
    {
        if (!m_DogTexture->IsReady()) return;

        u32 objectCount = 5;
        float scale = 0.25f;
        float offsetStep = 2.f / objectCount;
        float offsetStart = -1.f + 0.5f * scale;
        for (u32 i = 0; i < objectCount; i++)
        {
            ObjectData data;
            data.Offset = { offsetStart + offsetStep * i, offsetStart + offsetStep * i };
            data.Scale = { scale, scale };
            m_ObjectData->SetElements(&data, 1, i);
        }

        auto encoder1 = m_CommandBuffers[0]->EncodeRenderCommands(m_FrameTextureBuffers[0].get());
        encoder1->BindPipeline("object_image");
        encoder1->BindPipelineTextureResource(0, m_DogTexture.get());
        encoder1->BindPipelineBufferResource(1, m_ObjectData.get(), 0, 0, objectCount);
        encoder1->FlushPipelineBindings();
        encoder1->BindVertexBuffer(m_QuadVertices.get()); 
        encoder1->BindIndexBuffer(m_QuadIndices.get());
        encoder1->DrawIndexed(m_QuadIndices->GetAllocatedCount(), 0, 0, objectCount);
        encoder1->EndEncoding();

        auto frameEncoder = m_RenderContext->EncodeFrameRenderCommands();
        frameEncoder->BindPipeline("main");
        frameEncoder->BindPipelineTextureResource(0, m_FrameTextures[0].get());
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
        Flourish::ShaderCreateInfo shaderCreateInfo;
        Flourish::GraphicsPipelineCreateInfo gpCreateInfo;
        Flourish::ComputePipelineCreateInfo compCreateInfo;
        
        // Compute shader
        shaderCreateInfo.Type = Flourish::ShaderType::Compute;
        shaderCreateInfo.Source = R"(
            #version 460

            layout (local_size_x = 1) in;

            struct Object
            {
                vec2 Scale;
                vec2 Offset;
            };

            layout(binding = 0) buffer ObjectBuffer {
                Object data[];
            } objectBuffer;

            void main() {
                int objectCount = 5;
                float scale = 0.25f;
                float offsetStep = 2.f / objectCount;
                float offsetStart = -1.f + 0.5f * scale;
                uint id = gl_GlobalInvocationID.x;
                if (objectBuffer.data[id].Offset == vec2(0.f))
                    objectBuffer.data[id].Offset = vec2(offsetStart + offsetStep * id, offsetStart + offsetStep * id);
                else 
                    objectBuffer.data[id].Offset += vec2(0.0003f);
                objectBuffer.data[id].Scale = vec2(scale, scale);
            }
        )";
        auto computeShader = Flourish::Shader::Create(shaderCreateInfo);

        // Simple vert shader
        shaderCreateInfo.Type = Flourish::ShaderType::Vertex;
        shaderCreateInfo.Source = R"(
            #version 460

            layout(location = 0) in vec3 inPosition;
            layout(location = 1) in vec2 inTexCoord;

            layout(location = 0) out vec2 outTexCoord;

            void main() {
                gl_Position = vec4(inPosition, 1.f);
                outTexCoord = inTexCoord;
            }
        )";
        auto simpleVertShader = Flourish::Shader::Create(shaderCreateInfo);

        // Image frag shader
        shaderCreateInfo.Type = Flourish::ShaderType::Fragment;
        shaderCreateInfo.Source = R"(
            #version 460

            layout(location = 0) in vec2 inTexCoord;

            layout(location = 0) out vec4 outColor;

            layout(binding = 0) uniform sampler2D tex;

            void main() {
                outColor = texture(tex, inTexCoord);
            }
        )";
        auto imageFragShader = Flourish::Shader::Create(shaderCreateInfo);

        // Object vert shader
        shaderCreateInfo.Type = Flourish::ShaderType::Vertex;
        shaderCreateInfo.Source = R"(
            #version 460

            struct Object
            {
                vec2 Scale;
                vec2 Offset;
            };

            layout(location = 0) in vec3 inPosition;
            layout(location = 1) in vec2 inTexCoord;

            layout(location = 0) out vec2 outTexCoord;

            layout(binding = 1) readonly buffer ObjectBuffer {
                Object data[];
            } objectBuffer;

            void main() {
                uint instance = gl_InstanceIndex;
                vec2 finalPos = inPosition.xy * objectBuffer.data[instance].Scale;
                finalPos += objectBuffer.data[instance].Offset;
                gl_Position = vec4(finalPos, inPosition.z, 1.f);
                outTexCoord = inTexCoord;
            }
        )";
        auto objectVertShader = Flourish::Shader::Create(shaderCreateInfo);
        
        m_ComputeTarget = Flourish::ComputeTarget::Create();

        compCreateInfo.ComputeShader = computeShader;
        m_ComputePipeline = Flourish::ComputePipeline::Create(compCreateInfo);
        
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

        m_SimplePassNoDepth->CreatePipeline("simple_image", gpCreateInfo);

        gpCreateInfo.VertexShader = objectVertShader;
        m_SimplePassNoDepth->CreatePipeline("object_image", gpCreateInfo);
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
        
        // Object data
        {
            bufCreateInfo.Type = Flourish::BufferType::Storage;
            bufCreateInfo.Usage = Flourish::BufferUsageType::Dynamic;
            bufCreateInfo.Layout = {
                { Flourish::BufferDataType::Float2 },
                { Flourish::BufferDataType::Float2 }
            };
            bufCreateInfo.ElementCount = 1000;
            bufCreateInfo.InitialData = nullptr;
            m_ObjectData = Flourish::Buffer::Create(bufCreateInfo);
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
        texCreateInfo.AsyncCreation = true;
        if (imagePixels)
        {
            texCreateInfo.InitialData = imagePixels;
            texCreateInfo.InitialDataSize = imageWidth * imageHeight * 4;
        }
        texCreateInfo.SamplerState.AnisotropyEnable = false;
        m_DogTexture = Flourish::Texture::Create(texCreateInfo);
        if (imagePixels)
            delete[] imagePixels;

        imagePixels = stbi_load("resources/image2.jpg", &imageWidth, &imageHeight, &imageChannels, 4);
        texCreateInfo.Width = static_cast<u32>(imageWidth);
        texCreateInfo.Height = static_cast<u32>(imageHeight);
        texCreateInfo.InitialData = nullptr;
        texCreateInfo.InitialDataSize = 0;
        if (!imagePixels) { FL_LOG_WARN("Cat image failed to load"); }
        if (imagePixels)
        {
            texCreateInfo.InitialData = imagePixels;
            texCreateInfo.InitialDataSize = imageWidth * imageHeight * 4;
        }
        m_CatTexture = Flourish::Texture::Create(texCreateInfo);
        if (imagePixels)
            delete[] imagePixels;

        texCreateInfo.Width = m_ScreenWidth;
        texCreateInfo.Height = m_ScreenHeight;
        texCreateInfo.MipCount = 1;
        texCreateInfo.Channels = 4;
        texCreateInfo.DataType = Flourish::BufferDataType::UInt8;
        texCreateInfo.UsageType = Flourish::BufferUsageType::Dynamic;
        texCreateInfo.SamplerState.AnisotropyEnable = false;
        texCreateInfo.InitialData = nullptr;
        texCreateInfo.AsyncCreation = false;
        for (u32 i = 0; i < 10; i++)
            m_FrameTextures.push_back(Flourish::Texture::Create(texCreateInfo));
    }
    
    void Tests::CreateFramebuffers()
    {
        Flourish::FramebufferCreateInfo fbCreateInfo;
        fbCreateInfo.RenderPass = m_SimplePassNoDepth;
        fbCreateInfo.DepthAttachments = { {} };
        fbCreateInfo.Width = m_ScreenWidth;
        fbCreateInfo.Height = m_ScreenHeight;
        for (u32 i = 0; i < m_FrameTextures.size(); i++)
        {
            fbCreateInfo.ColorAttachments = { { { 0.f, 0.f, 0.f, 0.f }, m_FrameTextures[i] } };
            m_FrameTextureBuffers.push_back(Flourish::Framebuffer::Create(fbCreateInfo));
        }
    }
    
    void Tests::CreateCommandBuffers()
    {
        Flourish::CommandBufferCreateInfo cmdCreateInfo;
        
        for (u32 i = 0; i < 10; i++)
            m_CommandBuffers.push_back(Flourish::CommandBuffer::Create(cmdCreateInfo));
    }
}