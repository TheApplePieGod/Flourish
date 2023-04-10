#pragma once

#include "Flourish/Backends/Vulkan/Util/Common.h"

namespace Flourish::Vulkan
{
    class Shader;
    class DynamicOffsets
    {
    public:
        DynamicOffsets();

        void UpdateOffset(const Shader* shader, u32 setIndex, u32 bindingIndex, u32 offset);
        u32* GetOffsetData(const Shader* shader, u32 setIndex);
        void ResetOffsets(const Shader* shader);

    private:
        std::vector<u32> m_DynamicOffsets;
    };
}

