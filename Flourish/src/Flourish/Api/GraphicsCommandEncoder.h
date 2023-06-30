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
    };
}
