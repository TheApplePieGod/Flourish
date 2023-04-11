#pragma once

#include "Flourish/Backends/Vulkan/Util/Common.h"

namespace Flourish::Vulkan
{
    class Shader;
    class DescriptorSet;
    class DescriptorBinder
    {
    public:
        void Reset();
        void BindNewShader(const Shader* shader);

        // TODO: option to reset offsets after flushing

        /*
         * Assumes shader will always be set & valid
         */
        void BindDescriptorSet(const DescriptorSet* set, u32 setIndex);
        const DescriptorSet* GetDescriptorSet(u32 setIndex);
        void UpdateDynamicOffset(u32 setIndex, u32 bindingIndex, u32 offset);
        u32* GetDynamicOffsetData(u32 setIndex);

    private:
        std::vector<const DescriptorSet*> m_BoundSets;
        std::vector<u32> m_DynamicOffsets;
        const Shader* m_BoundShader;
    };
}

