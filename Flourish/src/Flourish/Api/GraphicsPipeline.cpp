#include "flpch.h"
#include "GraphicsPipeline.h"

#include "Flourish/Api/Context.h"

namespace Flourish
{
    bool AttachmentBlendState::operator==(const AttachmentBlendState& other) const
    {
        return BlendEnable == other.BlendEnable &&
               SrcColorBlendFactor == other.SrcColorBlendFactor &&
               DstColorBlendFactor == other.DstColorBlendFactor &&
               SrcAlphaBlendFactor == other.SrcAlphaBlendFactor &&
               DstAlphaBlendFactor == other.DstAlphaBlendFactor &&
               ColorBlendOperation == other.ColorBlendOperation &&
               AlphaBlendOperation == other.AlphaBlendOperation;
    }
}
