#pragma once

#include "Flourish/Api/CommandEncoder.h"

namespace Flourish
{
    class GraphicsCommandEncoder : public CommandEncoder
    {
    public:
        GraphicsCommandEncoder() = default;

        virtual void GenerateMipMaps(Flourish::Texture* texture) = 0;
    };
}