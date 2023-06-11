#pragma once

#include "Flourish/Api/RenderCommandEncoder.h"
#include "Flourish/Backends/Vulkan/Util/Common.h"
#include "Flourish/Backends/Vulkan/Util/Commands.h"
#include "Flourish/Backends/Vulkan/GraphicsPipeline.h"
#include "Flourish/Backends/Vulkan/Util/DescriptorBinder.h"

namespace Flourish::Vulkan
{
    class Framebuffer;
    class CommandBuffer;
    class Texture;
    class RenderCommandEncoder : public Flourish::RenderCommandEncoder 
    {
    public:
        RenderCommandEncoder() = default;
        RenderCommandEncoder(CommandBuffer* parentBuffer, bool frameRestricted);

        void BeginEncoding(Framebuffer* framebuffer);
        void EndEncoding() override;
        void BindPipeline(std::string_view pipelineName) override;
        void SetViewport(u32 x, u32 y, u32 width, u32 height) override;
        void SetScissor(u32 x, u32 y, u32 width, u32 height) override;
        void SetLineWidth(float width) override;
        void BindVertexBuffer(const Flourish::Buffer* buffer) override;
        void BindIndexBuffer(const Flourish::Buffer* buffer) override;
        void Draw(u32 vertexCount, u32 vertexOffset, u32 instanceCount, u32 instanceOffset) override;
        void DrawIndexed(u32 indexCount, u32 indexOffset, u32 vertexOffset, u32 instanceCount, u32 instanceOffset) override;
        void DrawIndexedIndirect(const Flourish::Buffer* indirectBuffer, u32 commandOffset, u32 drawCount) override;
        void StartNextSubpass() override;
        void ClearColorAttachment(u32 attachmentIndex) override;
        void ClearDepthAttachment() override;

        // TODO: flush should ensure all are bound

        void BindResourceSet(const Flourish::ResourceSet* set, u32 setIndex) override;
        void UpdateDynamicOffset(u32 setIndex, u32 bindingIndex, u32 offset) override;
        void FlushResourceSet(u32 setIndex) override;
        
        // TS
        inline VkCommandBuffer GetCommandBuffer() const { return m_CommandBuffer; }

    private:
        bool m_FrameRestricted;
        CommandBufferAllocInfo m_AllocInfo;
        VkCommandBuffer m_CommandBuffer;
        CommandBuffer* m_ParentBuffer;
        Framebuffer* m_BoundFramebuffer = nullptr;
        GraphicsPipeline* m_BoundPipeline = nullptr;
        std::string m_BoundPipelineName = "";
        DescriptorBinder m_DescriptorBinder;
        u32 m_SubpassIndex = 0;
    };
}
