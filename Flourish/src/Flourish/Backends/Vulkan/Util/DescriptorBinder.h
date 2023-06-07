#pragma once

#include "Flourish/Api/DescriptorSet.h"
#include "Flourish/Backends/Vulkan/Util/Common.h"

namespace Flourish::Vulkan
{
    class DescriptorPool;
    class DescriptorSet;
    class Shader;
    struct ReflectionDataElement;
    struct PipelineDescriptorData
    {
        struct SetData
        {
            bool Exists = false;
            std::vector<ReflectionDataElement> ReflectionData;
            std::shared_ptr<DescriptorPool> Pool;
            u32 DynamicOffsetIndex = 0; // When computing dynamic offsets
            u32 DynamicOffsetCount = 0;
        };

        std::vector<SetData> SetData;
        u32 TotalDynamicOffsets = 0;
        DescriptorSetPipelineCompatability Compatability;

        void Populate(Shader** shaders, u32 count);

        // TS
        std::shared_ptr<DescriptorSet> CreateDescriptorSet(u32 setIndex, DescriptorSetPipelineCompatability compatability, const DescriptorSetCreateInfo& createInfo);
    };

    class DescriptorSet;
    class DescriptorBinder
    {
    public:
        void Reset();
        void BindPipelineData(const PipelineDescriptorData* data);

        // TODO: option to reset offsets after flushing

        /*
         * Assumes pipeline data will always be set & valid
         */
        void BindDescriptorSet(const DescriptorSet* set, u32 setIndex);
        const DescriptorSet* GetDescriptorSet(u32 setIndex);
        void UpdateDynamicOffset(u32 setIndex, u32 bindingIndex, u32 offset);
        u32* GetDynamicOffsetData(u32 setIndex);

        inline bool DoesSetExist(u32 setIndex) const
        { return setIndex < m_BoundData->SetData.size() && m_BoundData->SetData[setIndex].Exists; }

        inline u32 GetDynamicOffsetCount(u32 setIndex) const { return m_BoundData->SetData[setIndex].DynamicOffsetCount; }

    private:
        std::vector<const DescriptorSet*> m_BoundSets;
        std::vector<u32> m_DynamicOffsets;
        const PipelineDescriptorData* m_BoundData;
    };
}

