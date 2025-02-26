#pragma once

#include "Flourish/Api/CommandEncoder.h"

namespace Flourish
{
    class Texture;
    class Buffer;
    class ResourceSet;
    class RenderCommandEncoder : public CommandEncoder
    {
    public:
        RenderCommandEncoder() = default;

        // All sets must be bound before drawing
        // TODO: pipeline ids?
        virtual void BindPipeline(std::string_view pipelineName) = 0;
        virtual void SetViewport(u32 x, u32 y, u32 width, u32 height) = 0;
        virtual void SetScissor(u32 x, u32 y, u32 width, u32 height) = 0;
        virtual void SetLineWidth(float width) = 0;
        virtual void BindVertexBuffer(const Buffer* buffer) = 0;
        virtual void BindIndexBuffer(const Buffer* buffer) = 0;
        virtual void Draw(u32 vertexCount, u32 vertexOffset, u32 instanceCount, u32 instanceOffset) = 0;
        virtual void DrawIndexed(u32 indexCount, u32 indexOffset, u32 vertexOffset, u32 instanceCount, u32 instanceOffset) = 0;
        virtual void DrawIndexedIndirect(const Buffer* indirectBuffer, u32 commandOffset, u32 drawCount) = 0;
        virtual void StartNextSubpass() = 0;
        virtual void ClearColorAttachment(u32 attachmentIndex) = 0;
        virtual void ClearDepthAttachment() = 0;
        
        // Bind -> Update -> Flush
        // Offset in bytes (make sure to respect buffer stride!)
        virtual void UpdateDynamicOffset(u32 setIndex, u32 bindingIndex, u32 offset) = 0;
        virtual void BindResourceSet(const ResourceSet* set, u32 setIndex) = 0;
        virtual void FlushResourceSet(u32 setIndex) = 0;
        virtual void PushConstants(u32 offset, u32 size, const void* data) = 0;

        virtual void WriteTimestamp(u32 timestampId) = 0;
    };
}
