#pragma once

#include "Flourish/Api/RenderCommandEncoder.h"
#include "Flourish/Backends/Vulkan/Util/Common.h"

namespace Flourish::Vulkan
{
    class Framebuffer;
    class CommandBuffer;
    class RenderCommandEncoder : public Flourish::RenderCommandEncoder 
    {
    public:
        RenderCommandEncoder(CommandBuffer* parentBuffer);
        ~RenderCommandEncoder();

        void BeginEncoding(Framebuffer* framebuffer);
        void EndEncoding() override;

        // TS
        void SetViewport(u32 x, u32 y, u32 width, u32 height) override;
        void SetScissor(u32 x, u32 y, u32 width, u32 height) override;
        void BindVertexBuffer(Flourish::Buffer* buffer) override;
        void BindIndexBuffer(Flourish::Buffer* buffer) override;
        void DrawIndexed(u32 indexCount, u32 indexOffset, u32 vertexOffset, u32 instanceCount) override;
        void Draw(u32 vertexCount, u32 vertexOffset, u32 instanceCount) override;
        
        // TS
        VkCommandBuffer GetCommandBuffer() const;

    private:
        std::thread::id m_AllocatedThread;
        std::array<VkCommandBuffer, Flourish::Context::MaxFrameBufferCount> m_CommandBuffers;
        CommandBuffer* m_ParentBuffer;
    };
}