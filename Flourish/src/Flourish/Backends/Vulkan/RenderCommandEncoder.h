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
        void PushConstants(u32 offset, u32 size, const void* data) override;

        void WriteTimestamp(u32 timestampId) override;
        
        // TS
        inline VkCommandBuffer GetCommandBuffer() const { return m_CurrentCommandBuffer; }
        inline void MarkManuallyRecorded() { m_AnyCommandRecorded = true; }

    private:
        void InitializeSubpass();

    private:
        bool m_AnyCommandRecorded = false;
        bool m_FrameRestricted;
        CommandBufferEncoderSubmission m_Submission;
        VkCommandBuffer m_CurrentCommandBuffer;
        CommandBuffer* m_ParentBuffer;
        GraphicsPipeline* m_BoundPipeline = nullptr;
        std::string m_BoundPipelineName = "";
        DescriptorBinder m_DescriptorBinder;
        u32 m_SubpassIndex = 0;
    };
}
