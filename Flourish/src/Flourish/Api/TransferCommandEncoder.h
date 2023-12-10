#pragma once

#include "Flourish/Api/CommandEncoder.h"

namespace Flourish
{
    class Texture;
    class Buffer;
    class TransferCommandEncoder : public CommandEncoder
    {
    public:
        TransferCommandEncoder() = default;

        virtual void FlushBuffer(Buffer* buffer) = 0;
        virtual void CopyTextureToBuffer(Texture* texture, Buffer* buffer, u32 layerIndex = 0, u32 mipLevel = 0) = 0;
        virtual void CopyBufferToTexture(Texture* texture, Buffer* buffer, u32 layerIndex = 0, u32 mipLevel = 0) = 0;
        virtual void CopyBufferToBuffer(Buffer* src, Buffer* dst, u32 srcOffset, u32 dstOffset, u32 size) = 0;
    };
}
