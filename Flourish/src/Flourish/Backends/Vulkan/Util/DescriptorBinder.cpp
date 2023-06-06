#include "flpch.h"
#include "DescriptorBinder.h"

#include "Flourish/Backends/Vulkan/DescriptorSet.h"
#include "Flourish/Backends/Vulkan/Shader.h"
#include "Flourish/Backends/Vulkan/Util/DescriptorPool.h"

namespace Flourish::Vulkan
{
    void PipelineDescriptorData::Populate(Shader** shaders, u32 count)
    {
        TotalDynamicOffsets = 0;
        SetData.clear();

        for (u32 i = 0; i < count; i++)
        {
            for (auto& elem : shaders[i]->GetReflectionData())
            {
                if (SetData.size() <= elem.SetIndex)
                    SetData.resize(elem.SetIndex + 1);
                auto& set = SetData[elem.SetIndex];
                set.Exists = true;
                set.ReflectionData.emplace_back(elem);
                if (elem.ResourceType == ShaderResourceType::StorageBuffer || elem.ResourceType == ShaderResourceType::UniformBuffer)
                    set.DynamicOffsetCount++;
            }
        }
        
        // Ensure there are no duplicate binding indices within sets and sort each set
        for (auto& set : SetData)
        {
            if (!set.Exists)
                continue;

            FL_ASSERT(!set.ReflectionData.empty(), "Cannot have an empty descriptor set");

            set.Pool = std::make_shared<DescriptorPool>(set.ReflectionData);

            #ifdef FL_DEBUG
            for (auto& check : set.ReflectionData)
                for (auto& elem : set.ReflectionData)
                    if (check.BindingIndex == elem.BindingIndex)
                        FL_ASSERT(check.UniqueId == elem.UniqueId, "Binding index must be unique for all shader resources");
            #endif

            std::sort(
                set.ReflectionData.begin(),
                set.ReflectionData.end(),
                [](const ReflectionDataElement& a, const ReflectionDataElement& b)
                {
                    return a.BindingIndex < b.BindingIndex;
                }
            );

            // Update dynamic offsets offset index
            set.DynamicOffsetIndex = TotalDynamicOffsets;
            TotalDynamicOffsets += set.DynamicOffsetCount;
        }
    }

    std::shared_ptr<DescriptorSet> PipelineDescriptorData::CreateDescriptorSet(const DescriptorSetCreateInfo& createInfo)
    {
        u32 setIndex = createInfo.SetIndex;

        if (setIndex >= SetData.size() || !SetData[setIndex].Exists)
        {
            FL_ASSERT(false, "AllocateDescriptorSet invalid set index");
            return nullptr;
        }

        auto& data = SetData[setIndex];
        return std::make_shared<DescriptorSet>(createInfo, data.Pool);
    }

    void DescriptorBinder::Reset()
    {
        m_BoundData = nullptr;
    }

    void DescriptorBinder::BindDescriptorSet(const DescriptorSet* set, u32 setIndex)
    {
        FL_CRASH_ASSERT(setIndex < m_BoundSets.size(), "Set index out of range");
        FL_CRASH_ASSERT(m_BoundData->SetData[setIndex].Exists, "Set index does not exist in the shader");

        #ifdef FL_DEBUG
        FL_ASSERT(
            m_BoundData->SetData[setIndex].Pool->CheckCompatibility(set->GetParentPool()),
            "Trying to bind incompatible descriptor set"
        );
        #endif
        
        m_BoundSets[setIndex] = set;
    }

    const DescriptorSet* DescriptorBinder::GetDescriptorSet(u32 setIndex)
    {
        FL_CRASH_ASSERT(setIndex < m_BoundSets.size(), "Set index out of range");
        FL_CRASH_ASSERT(m_BoundData->SetData[setIndex].Exists, "Set index does not exist in the shader");

        return m_BoundSets[setIndex];
    }

    void DescriptorBinder::UpdateDynamicOffset(u32 setIndex, u32 bindingIndex, u32 offset)
    {
        FL_CRASH_ASSERT(setIndex < m_BoundSets.size(), "Set index out of range");
        FL_CRASH_ASSERT(m_BoundSets[setIndex], "Must bind set before calling UpdateDynamicOffset");

        const auto& data = m_BoundData->SetData[setIndex];
        const auto& pool = m_BoundSets[setIndex]->GetParentPool();
        
        if (!pool->DoesBindingExist(bindingIndex))
        {
            FL_LOG_ERROR("Attempting to update dynamic offset for descriptor binding %d that doesn't exist in the set", bindingIndex);
            throw std::exception();
        }
        
        // TODO: validate total offset ???
        m_DynamicOffsets[
            data.DynamicOffsetIndex +
            pool->GetBindingData()[bindingIndex].BufferArrayIndex
        ] = offset;
    }

    // TODO: set bounds checking
    u32* DescriptorBinder::GetDynamicOffsetData(u32 setIndex)
    {
        FL_CRASH_ASSERT(setIndex < m_BoundSets.size(), "Set index out of range");

        return m_DynamicOffsets.data() + m_BoundData->SetData[setIndex].DynamicOffsetIndex;
    }

    void DescriptorBinder::BindPipelineData(const PipelineDescriptorData* data)
    {
        if (!m_DynamicOffsets.empty())
            memset(m_DynamicOffsets.data(), 0, m_DynamicOffsets.size() * sizeof(u32));
        m_DynamicOffsets.resize(data->TotalDynamicOffsets);

        if (!m_BoundSets.empty())
            memset(m_BoundSets.data(), 0, m_BoundSets.size() * sizeof(void*));
        m_BoundSets.resize(data->SetData.size());

        m_BoundData = data;
    }
}
