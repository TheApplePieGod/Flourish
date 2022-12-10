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
        RenderCommandEncoder(CommandBuffer* parentBuffer);
        ~RenderCommandEncoder();

        void BeginEncoding(Framebuffer* framebuffer);
        void EndEncoding() override;
        void BindPipeline(std::string_view pipelineName) override;
        void SetViewport(u32 x, u32 y, u32 width, u32 height) override;
        void SetScissor(u32 x, u32 y, u32 width, u32 height) override;
        void BindVertexBuffer(Flourish::Buffer* buffer) override;
        void BindIndexBuffer(Flourish::Buffer* buffer) override;
        void Draw(u32 vertexCount, u32 vertexOffset, u32 instanceCount) override;
        void DrawIndexed(u32 indexCount, u32 indexOffset, u32 vertexOffset, u32 instanceCount) override;
        void DrawIndexedIndirect(Flourish::Buffer* indirectBuffer, u32 commandOffset, u32 drawCount) override;
        void StartNextSubpass() override;
        void ClearColorAttachment(u32 attachmentIndex) override;
        void ClearDepthAttachment() override;
        
        void BindPipelineBufferResource(u32 bindingIndex, Flourish::Buffer* buffer, u32 bufferOffset, u32 dynamicOffset, u32 elementCount) override;
        void BindPipelineTextureResource(u32 bindingIndex, Flourish::Texture* texture) override;
        void BindPipelineTextureLayerResource(u32 bindingIndex, Flourish::Texture* texture, u32 layerIndex, u32 mipLevel) override;
        void BindPipelineSubpassInputResource(u32 bindingIndex, SubpassAttachment attachment) override;
        void FlushPipelineBindings() override;
        
        // TS
        VkCommandBuffer GetCommandBuffer() const;

    private:
        void ValidatePipelineBinding(u32 bindingIndex, ShaderResourceType resourceType, void* resource);

    private:
        std::array<VkCommandBuffer, Flourish::Context::MaxFrameBufferCount> m_CommandBuffers;
        CommandBufferAllocInfo m_AllocInfo;
        CommandBuffer* m_ParentBuffer;
        Framebuffer* m_BoundFramebuffer = nullptr;
        DescriptorSet* m_BoundDescriptorSet = nullptr;
        GraphicsPipeline* m_BoundPipeline = nullptr;
        std::string m_BoundPipelineName = "";
        u32 m_SubpassIndex = 0;
    };
}