#pragma once

#include "Flourish/Api/Context.h"
#include "Flourish/Api/RenderContext.h"
#include "Flourish/Api/CommandBuffer.h"
#include "Flourish/Api/Buffer.h"
#include "Flourish/Api/RenderPass.h"
#include "Flourish/Api/Texture.h"
#include "Flourish/Api/Framebuffer.h"
#include "Flourish/Api/RenderCommandEncoder.h"

namespace FlourishTesting
{
    class Tests
    {
    public:
        Tests();
        
        void Run();
            
    private:
        struct Vertex
        {
            float Position[3];
            float TexCoord[2];
        };
        
        struct ObjectData
        {
            float Scale[2];
            float Offset[2];
        };
    
    private:
        void RunSingleThreadedTest();

        void CreateRenderPasses();
        void CreatePipelines();
        void CreateBuffers();
        void CreateTextures();
        void CreateFramebuffers();
        void CreateCommandBuffers();
        
    private:
        const u32 m_ScreenWidth = 1920;
        const u32 m_ScreenHeight = 1080;
        const Flourish::BufferLayout m_VertexLayout = {
            { Flourish::BufferDataType::Float3 },
            { Flourish::BufferDataType::Float2 }
        }; 

        // TODO: multiple render contexts
        std::shared_ptr<Flourish::RenderContext> m_RenderContext;

        std::shared_ptr<Flourish::RenderPass> m_SimplePassNoDepth;

        std::shared_ptr<Flourish::Framebuffer> m_SimplePassNoDepthFrameTexFB;
        
        std::shared_ptr<Flourish::Buffer> m_FullTriangleVertices;
        std::shared_ptr<Flourish::Buffer> m_QuadVertices;
        std::shared_ptr<Flourish::Buffer> m_QuadIndices;
        std::shared_ptr<Flourish::Buffer> m_ObjectData;
        
        std::shared_ptr<Flourish::Texture> m_DogTexture;
        std::shared_ptr<Flourish::Texture> m_UIntFrameTex;
        
        std::vector<std::shared_ptr<Flourish::CommandBuffer>> m_CommandBuffers;
    };
}