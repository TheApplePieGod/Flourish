#pragma once

#include "Flourish/Api/CommandEncoder.h"

namespace Flourish
{
    class Texture;
    class Buffer;
    class RenderCommandEncoder : public CommandEncoder
    {
    public:
        RenderCommandEncoder() = default;

        // All shader resources must be bound before drawing
        // TODO: pipeline ids?
        virtual void BindPipeline(std::string_view pipelineName) = 0;
        virtual void SetViewport(u32 x, u32 y, u32 width, u32 height) = 0;
        virtual void SetScissor(u32 x, u32 y, u32 width, u32 height) = 0;
        virtual void SetLineWidth(float width) = 0;
        virtual void BindVertexBuffer(Buffer* buffer) = 0;
        virtual void BindIndexBuffer(Buffer* buffer) = 0;
        virtual void Draw(u32 vertexCount, u32 vertexOffset, u32 instanceCount, u32 instanceOffset) = 0;
        virtual void DrawIndexed(u32 indexCount, u32 indexOffset, u32 vertexOffset, u32 instanceCount, u32 instanceOffset) = 0;
        virtual void DrawIndexedIndirect(Buffer* indirectBuffer, u32 commandOffset, u32 drawCount) = 0;
        virtual void StartNextSubpass() = 0;
        virtual void ClearColorAttachment(u32 attachmentIndex) = 0;
        virtual void ClearDepthAttachment() = 0;
        
        // Buffer offset refers to the element starting point in the buffer and dynamicOffset refers to a dynamic element offset
        virtual void BindPipelineBufferResource(u32 bindingIndex, Buffer* buffer, u32 bufferOffset, u32 dynamicOffset, u32 elementCount) = 0;
        virtual void BindPipelineTextureResource(u32 bindingIndex, Texture* texture) = 0;
        virtual void BindPipelineTextureLayerResource(u32 bindingIndex, Flourish::Texture* texture, u32 layerIndex, u32 mipLevel) = 0;
        virtual void BindPipelineSubpassInputResource(u32 bindingIndex, SubpassAttachment attachment) = 0;
        virtual void FlushPipelineBindings() = 0;
    };
}