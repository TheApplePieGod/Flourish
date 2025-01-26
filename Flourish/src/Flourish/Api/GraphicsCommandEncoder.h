#pragma once

#include "Flourish/Api/Texture.h"
#include "Flourish/Api/CommandEncoder.h"

namespace Flourish
{
    class GraphicsCommandEncoder : public CommandEncoder
    {
    public:
        GraphicsCommandEncoder() = default;

        virtual void GenerateMipMaps(Flourish::Texture* texture, SamplerFilter filter) = 0;
        virtual void BlitTexture(Texture* src, Texture* dst, u32 srcLayerIndex, u32 srcMipLevel, u32 dstLayerIndex, u32 dstMipLevel) = 0;
        
        virtual void WriteTimestamp(u32 timestampId) = 0;
    };
}
