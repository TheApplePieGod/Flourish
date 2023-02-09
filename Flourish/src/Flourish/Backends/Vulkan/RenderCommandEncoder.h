#pragma once

#include "Flourish/Api/RenderCommandEncoder.h"
#include "Flourish/Backends/Vulkan/Util/Common.h"
#include "Flourish/Backends/Vulkan/Util/DescriptorSet.h"
#include "Flourish/Backends/Vulkan/Util/Commands.h"
#include "Flourish/Backends/Vulkan/GraphicsPipeline.h"

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
        
        void BindPipelineBufferResource(u32 bindingIndex, const Flourish::Buffer* buffer, u32 bufferOffset, u32 dynamicOffset, u32 elementCount) override;
        void BindPipelineTextureResource(u32 bindingIndex, const Flourish::Texture* texture) override;
        void BindPipelineTextureLayerResource(u32 bindingIndex, const Flourish::Texture* texture, u32 layerIndex, u32 mipLevel) override;
        void BindPipelineSubpassInputResource(u32 bindingIndex, SubpassAttachment attachment) override;
        void FlushPipelineBindings() override;

        // TS
        inline VkCommandBuffer GetCommandBuffer() const { return m_CommandBuffer; }

    private:
        void ValidatePipelineBinding(u32 bindingIndex, ShaderResourceType resourceType, const void* resource);

    private:
        bool m_FrameRestricted;
        CommandBufferAllocInfo m_AllocInfo;
        VkCommandBuffer m_CommandBuffer;
        CommandBuffer* m_ParentBuffer;
        Framebuffer* m_BoundFramebuffer = nullptr;
        DescriptorSet* m_BoundDescriptorSet = nullptr;
        GraphicsPipeline* m_BoundPipeline = nullptr;
        std::string m_BoundPipelineName = "";
        u32 m_SubpassIndex = 0;
    };
}