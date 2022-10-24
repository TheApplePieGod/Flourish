#pragma once

#include "Flourish/Api/Framebuffer.h"

namespace Flourish
{
    class RenderCommandEncoder  
    {
    public:
        RenderCommandEncoder() = default;

        virtual void EndEncoding() = 0;

        // All shader resources must be bound before drawing
        // TODO: pipeline ids?
        virtual void BindPipeline(const std::string_view pipelineName) = 0;
        virtual void SetViewport(u32 x, u32 y, u32 width, u32 height) = 0;
        virtual void SetScissor(u32 x, u32 y, u32 width, u32 height) = 0;
        virtual void BindVertexBuffer(Buffer* buffer) = 0;
        virtual void BindIndexBuffer(Buffer* buffer) = 0;
        virtual void Draw(u32 vertexCount, u32 vertexOffset, u32 instanceCount) = 0;
        virtual void DrawIndexed(u32 indexCount, u32 indexOffset, u32 vertexOffset, u32 instanceCount) = 0;
        virtual void DrawIndexedIndirect(Buffer* indirectBuffer, u32 commandOffset, u32 drawCount) = 0;
        
        virtual void BindPipelineBufferResource(u32 bindingIndex, Buffer* buffer, u32 elementOffset, u32 elementCount) = 0;

        // TS
        inline bool IsEncoding() const { return m_Encoding; }
        
    protected:
        bool m_Encoding = false;
    };
}