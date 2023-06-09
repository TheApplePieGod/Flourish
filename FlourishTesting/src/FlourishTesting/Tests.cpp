#include "Flourish/Api/DescriptorSet.h"
#include "Flourish/Api/Shader.h"
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
        while (true) //(Flourish::Context::FrameCount() < 100)
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
                    encoder->BindDescriptorSet(m_DogDescriptorSet.get(), 0);
                else
                    encoder->BindDescriptorSet(m_CatDescriptorSet.get(), 0);
                encoder->FlushDescriptorSet(0);
                encoder->BindVertexBuffer(m_FullTriangleVertices.get()); 
                encoder->Draw(3, 0, 1, 0);
                encoder->EndEncoding();
            }));
            
            parallelBuffers.push_back(m_CommandBuffers[i].get());
        }
        
        {
            jobs.push_back(std::async(std::launch::async, [&]()
            {
                auto encoder = m_CommandBuffers[objectCount]->EncodeComputeCommands();
                encoder->BindPipeline(m_ComputePipeline.get());
                encoder->BindDescriptorSet(m_ObjectDescriptorSet.get(), 0);
                encoder->FlushDescriptorSet(0);
                encoder->Dispatch(objectCount, 1, 1);
                encoder->EndEncoding();
            }));
            
            parallelBuffers.push_back(m_CommandBuffers[objectCount].get());
        }
        
        auto encoder = m_CommandBuffers[objectCount + 1]->EncodeRenderCommands(m_FrameTextureBuffers[objectCount].get());
        encoder->BindPipeline("object_image");
        encoder->BindDescriptorSet(m_ObjectDescriptorSetDynamic.get(), 1);
        for (u32 i = 0; i < objectCount; i++)
        {
            m_FrameDescriptorSet->BindTexture(0, m_FrameTextures[i]);
            m_FrameDescriptorSet->FlushBindings();
            encoder->BindDescriptorSet(m_FrameDescriptorSet.get(), 0);
            encoder->FlushDescriptorSet(0);
            encoder->UpdateDynamicOffset(1, 0, i * m_ObjectData->GetStride());
            encoder->FlushDescriptorSet(1);
            encoder->BindVertexBuffer(m_QuadVertices.get()); 
            encoder->BindIndexBuffer(m_QuadIndices.get());
            encoder->DrawIndexed(m_QuadIndices->GetAllocatedCount(), 0, 0, 1, 0);
        }
        encoder->EndEncoding();

        auto frameEncoder = m_RenderContext->EncodeRenderCommands();
        frameEncoder->BindPipeline("main");
        m_FrameDescriptorSet->BindTexture(0, m_FrameTextures[objectCount]);
        m_FrameDescriptorSet->FlushBindings();
        frameEncoder->BindDescriptorSet(m_FrameDescriptorSet.get(), 0);
        frameEncoder->FlushDescriptorSet(0);
        frameEncoder->BindVertexBuffer(m_FullTriangleVertices.get()); // TODO: validate buffer is actually a vertex
        frameEncoder->Draw(3, 0, 1, 0);
        frameEncoder->EndEncoding();
        
        for (auto& job : jobs)
            job.wait();

        m_RenderContext->Present({{ parallelBuffers, { m_CommandBuffers[objectCount + 1].get() } }});
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
        encoder1->BindDescriptorSet(m_DogDescriptorSet.get(), 0);
        encoder1->FlushDescriptorSet(0);
        encoder1->BindDescriptorSet(m_ObjectDescriptorSet.get(), 1);
        encoder1->FlushDescriptorSet(1);
        encoder1->BindVertexBuffer(m_QuadVertices.get()); 
        encoder1->BindIndexBuffer(m_QuadIndices.get());
        encoder1->DrawIndexed(m_QuadIndices->GetAllocatedCount(), 0, 0, objectCount, 0);
        encoder1->EndEncoding();

        auto frameEncoder = m_RenderContext->EncodeRenderCommands();
        frameEncoder->BindPipeline("main");
        m_FrameDescriptorSet->BindTexture(0, m_FrameTextures[0]);
        m_FrameDescriptorSet->FlushBindings();
        frameEncoder->FlushDescriptorSet(0);
        frameEncoder->BindVertexBuffer(m_FullTriangleVertices.get()); // TODO: validate buffer is actually a vertex
        frameEncoder->Draw(3, 0, 1, 0);
        frameEncoder->EndEncoding();

        m_RenderContext->Present({{ { m_CommandBuffers[0].get() } }});
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
        CreateTextures();
        CreateBuffers();
        CreatePipelines();
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
        rpCreateInfo.DepthAttachments = {
            { Flourish::ColorFormat::Depth }
        };
        rpCreateInfo.Subpasses = {
            { {}, { { Flourish::SubpassAttachmentType::Color, 0 } } }
        };
        rpCreateInfo.SampleCount = Flourish::MsaaSampleCount::Four;
        m_SimplePassNoDepth = Flourish::RenderPass::Create(rpCreateInfo);
    }

    void Tests::CreatePipelines()
    {
        Flourish::ShaderCreateInfo shaderCreateInfo;
        Flourish::GraphicsPipelineCreateInfo gpCreateInfo;
        Flourish::ComputePipelineCreateInfo compCreateInfo;
        
        // Compute shader
        shaderCreateInfo.Type = Flourish::ShaderTypeFlags::Compute;
        shaderCreateInfo.Source = R"(
            #version 460

            layout (local_size_x = 1) in;

            struct Object
            {
                vec2 Scale;
                vec2 Offset;
            };

            layout(binding = 0, set = 0) buffer ObjectBuffer {
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
        shaderCreateInfo.Type = Flourish::ShaderTypeFlags::Vertex;
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
        shaderCreateInfo.Type = Flourish::ShaderTypeFlags::Fragment;
        shaderCreateInfo.Source = R"(
            #version 460

            layout(location = 0) in vec2 inTexCoord;

            layout(location = 0) out vec4 outColor;

            layout(binding = 0, set = 0) uniform sampler2D tex;

            void main() {
                outColor = texture(tex, inTexCoord);
            }
        )";
        auto imageFragShader = Flourish::Shader::Create(shaderCreateInfo);

        // Object vert shader
        shaderCreateInfo.Type = Flourish::ShaderTypeFlags::Vertex;
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

            layout(binding = 0, set = 1) readonly buffer ObjectBuffer {
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
        auto mainPipeline = m_RenderContext->GetRenderPass()->CreatePipeline("main", gpCreateInfo);

        m_SimplePassNoDepth->CreatePipeline("simple_image", gpCreateInfo);

        gpCreateInfo.VertexShader = objectVertShader;
        auto objectPipeline = m_SimplePassNoDepth->CreatePipeline("object_image", gpCreateInfo);

        Flourish::DescriptorSetCreateInfo descCreateInfo;
        descCreateInfo.Writability = Flourish::DescriptorSetWritability::OnceStaticData;
        m_DogDescriptorSet = mainPipeline->CreateDescriptorSet(0, descCreateInfo);
        m_DogDescriptorSet->BindTexture(0, m_DogTexture);
        m_DogDescriptorSet->FlushBindings();
        m_CatDescriptorSet = mainPipeline->CreateDescriptorSet(0, descCreateInfo);
        m_CatDescriptorSet->BindTexture(0, m_CatTexture);
        m_CatDescriptorSet->FlushBindings();
        descCreateInfo.Writability = Flourish::DescriptorSetWritability::OnceDynamicData;
        m_ObjectDescriptorSet = m_ComputePipeline->CreateDescriptorSet(0, descCreateInfo);
        m_ObjectDescriptorSet->BindBuffer(0, m_ObjectData, 0, m_ObjectData->GetAllocatedCount());
        m_ObjectDescriptorSet->FlushBindings();
        m_ObjectDescriptorSetDynamic = objectPipeline->CreateDescriptorSet(1, descCreateInfo);
        m_ObjectDescriptorSetDynamic->BindBuffer(0, m_ObjectData, 0, 1);
        m_ObjectDescriptorSetDynamic->FlushBindings();
        descCreateInfo.Writability = Flourish::DescriptorSetWritability::MultiPerFrame;
        m_FrameDescriptorSet = mainPipeline->CreateDescriptorSet(0, descCreateInfo);
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
        texCreateInfo.Format = Flourish::ColorFormat::RGBA8_UNORM;
        texCreateInfo.Usage = Flourish::TextureUsageType::Readonly;
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
        texCreateInfo.Format = Flourish::ColorFormat::RGBA8_UNORM;
        texCreateInfo.Usage = Flourish::TextureUsageType::RenderTarget;
        texCreateInfo.Writability = Flourish::TextureWritability::PerFrame;
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
        cmdCreateInfo.MaxEncoders = 2;
        
        for (u32 i = 0; i < 10; i++)
            m_CommandBuffers.push_back(Flourish::CommandBuffer::Create(cmdCreateInfo));
    }
}
