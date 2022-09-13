#pragma once

#include "Flourish/Api/Framebuffer.h"

namespace Flourish
{
    struct RenderCommandEncoderCreateInfo
    {
        std::shared_ptr<Framebuffer> Framebuffer;
        bool Reusable = false; // True if encoded commands might be used more than once
    };
     
    class RenderCommandEncoder  
    {
    public:
        RenderCommandEncoder(const RenderCommandEncoderCreateInfo& createInfo)
        {}
        virtual ~RenderCommandEncoder() = default;

        virtual void BeginEncoding() = 0;
        virtual void EndEncoding() = 0;

        // TS
        // All shader resources must be bound before drawing
        virtual void SetViewport(u32 x, u32 y, u32 width, u32 height) = 0;
        virtual void BindVertexBuffer(Buffer* buffer) = 0;
        virtual void BindIndexBuffer(Buffer* buffer) = 0;
        virtual void DrawIndexed(u32 indexCount, u32 indexOffset, u32 vertexOffset, u32 instanceCount) = 0;
        virtual void Draw(u32 vertexCount, u32 vertexOffset, u32 instanceCount) = 0;
        // virtual void DrawIndexedIndirect(Buffer* indirectBuffer, u32 commandOffset, u32 drawCount) = 0;

    public:
        // TS
        static std::shared_ptr<RenderCommandEncoder> Create(const RenderCommandEncoderCreateInfo& createInfo);

    protected:
        RenderCommandEncoderCreateInfo m_Info;
        bool m_Encoding = false;
    };
}