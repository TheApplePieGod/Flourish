#pragma once

#include "Flourish/Api/PipelineCommon.h"
#include "Flourish/Api/ResourceSet.h"
#include "Flourish/Backends/Vulkan/Util/Common.h"

namespace Flourish::Vulkan
{
    class DescriptorPool;
    class ResourceSet;
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
        ResourceSetPipelineCompatability Compatability;
        VkPushConstantRange PushConstantRange{};

        void Populate(Shader** shaders, u32 count, const std::vector<AccessFlagsOverride>& accessOverrides);

        // TS
        std::shared_ptr<ResourceSet> CreateResourceSet(u32 setIndex, ResourceSetPipelineCompatability compatability, const ResourceSetCreateInfo& createInfo);
    };

    struct PipelineSpecializationHelper
    {
        std::vector<u8> SpecData;
        std::vector<VkSpecializationInfo> SpecInfos;
        std::vector<VkSpecializationMapEntry> MapEntries;

        void Populate(Shader** shaders, std::vector<SpecializationConstant>* specs, u32 count);
    };

    class ResourceSet;
    class DescriptorBinder
    {
    public:
        void Reset();
        void BindPipelineData(const PipelineDescriptorData* data);

        // TODO: option to reset offsets after flushing

        /*
         * Assumes pipeline data will always be set & valid
         */
        void BindResourceSet(const ResourceSet* set, u32 setIndex);
        const ResourceSet* GetResourceSet(u32 setIndex);
        void UpdateDynamicOffset(u32 setIndex, u32 bindingIndex, u32 offset);
        u32* GetDynamicOffsetData(u32 setIndex);

        inline bool DoesSetExist(u32 setIndex) const
        { return setIndex < m_BoundData->SetData.size() && m_BoundData->SetData[setIndex].Exists; }

        inline u32 GetDynamicOffsetCount(u32 setIndex) const { return m_BoundData->SetData[setIndex].DynamicOffsetCount; }
        inline auto GetBoundData() const { return m_BoundData; }

    private:
        std::vector<const ResourceSet*> m_BoundSets;
        std::vector<u32> m_DynamicOffsets;
        const PipelineDescriptorData* m_BoundData;
    };
}

