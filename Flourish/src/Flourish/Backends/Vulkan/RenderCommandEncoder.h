#pragma once

#include "Flourish/Api/RenderCommandEncoder.h"
#include "Flourish/Backends/Vulkan/Util/Common.h"
#include "Flourish/Backends/Vulkan/CommandBuffer.h"

namespace Flourish::Vulkan
{
    class RenderCommandEncoder : public Flourish::RenderCommandEncoder 
    {
    public:
        RenderCommandEncoder(const RenderCommandEncoderCreateInfo& createInfo);
        ~RenderCommandEncoder() override;

        void BeginEncoding() override;
        void EndEncoding() override;

        // TS
        void SetViewport(u32 x, u32 y, u32 width, u32 height) override;
        void SetScissor(u32 x, u32 y, u32 width, u32 height) override;
        void BindVertexBuffer(Flourish::Buffer* buffer) override;
        void BindIndexBuffer(Flourish::Buffer* buffer) override;
        void DrawIndexed(u32 indexCount, u32 indexOffset, u32 vertexOffset, u32 instanceCount) override;
        void Draw(u32 vertexCount, u32 vertexOffset, u32 instanceCount) override;
        
        // TS
        const VkRenderPassBeginInfo& GetRenderPassBeginInfo();
        inline VkCommandBuffer GetCommandBuffer() const { return m_CommandBuffer.GetCommandBuffer(); }

    private:
        CommandBuffer m_CommandBuffer;
        VkRenderPassBeginInfo m_RenderPassBeginInfo{};
    };
}