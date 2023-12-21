#include "flpch.h"
#include "DescriptorBinder.h"

#include "Flourish/Backends/Vulkan/Context.h"
#include "Flourish/Backends/Vulkan/ResourceSet.h"
#include "Flourish/Backends/Vulkan/Shader.h"
#include "Flourish/Backends/Vulkan/Util/DescriptorPool.h"

namespace Flourish::Vulkan
{
    void PipelineDescriptorData::Populate(Shader** shaders, u32 count, const std::vector<AccessFlagsOverride>& accessOverrides)
    {
        TotalDynamicOffsets = 0;
        SetData.clear();

        for (u32 i = 0; i < count; i++)
        {
            // Descriptor bindings
            for (auto& elem : shaders[i]->GetReflectionData())
            {
                if (SetData.size() <= elem.SetIndex)
                    SetData.resize(elem.SetIndex + 1);
                auto& set = SetData[elem.SetIndex];
                set.Exists = true;

                // Check the set for duplicate bindings. Duplicates are most likely
                // OK if they look similar. We could in the future improve debug
                // analysis. Regardless, duplicate bindings with the same data should
                // not be added twice.
                u32 bindingIndex = elem.BindingIndex;
                auto found = std::find_if(
                    set.ReflectionData.begin(),
                    set.ReflectionData.end(),
                    [bindingIndex](const auto& check) { return check.BindingIndex == bindingIndex; }
                );
                if (found != set.ReflectionData.end())
                {
                    FL_ASSERT(
                        (found->Size == elem.Size &&
                         found->ArrayCount == elem.ArrayCount),
                        "Binding index must be unique for all shader resources"
                    );

                    found->AccessType |= elem.AccessType;

                    continue;
                }

                set.ReflectionData.emplace_back(elem);

                if (elem.ResourceType == ShaderResourceType::StorageBuffer || elem.ResourceType == ShaderResourceType::UniformBuffer)
                    set.DynamicOffsetCount++;
            }

            // Push constants
            auto& push = shaders[i]->GetPushConstantReflection();
            if (push.Size == 0)
                continue;
            FL_ASSERT(
                PushConstantRange.size == 0 || push.Size == PushConstantRange.size,
                "Push constants must be the same for all shaders in pipeline"
            );
            PushConstantRange.size = push.Size;
            PushConstantRange.stageFlags |= Common::ConvertShaderAccessType(push.AccessType);
        }

        // Process overrides
        for (auto& override : accessOverrides)
        {
            if (override.SetIndex >= SetData.size())
                continue;
            auto& set = SetData[override.SetIndex];
            if (!set.Exists)
                continue;
            for (auto& elem : set.ReflectionData)
                if (elem.BindingIndex == override.BindingIndex)
                    elem.AccessType = override.Flags;
        }
        
        // Ensure there are no duplicate binding indices within sets and sort each set
        for (auto& set : SetData)
        {
            if (!set.Exists)
                continue;

            FL_ASSERT(!set.ReflectionData.empty(), "Cannot have an empty resource set");

            std::sort(
                set.ReflectionData.begin(),
                set.ReflectionData.end(),
                [](const ReflectionDataElement& a, const ReflectionDataElement& b)
                {
                    return a.BindingIndex < b.BindingIndex;
                }
            );

            set.Pool = std::make_shared<DescriptorPool>(set.ReflectionData);

            // Update dynamic offsets offset index
            set.DynamicOffsetIndex = TotalDynamicOffsets;
            TotalDynamicOffsets += set.DynamicOffsetCount;
        }
    }

    std::shared_ptr<ResourceSet> PipelineDescriptorData::CreateResourceSet(u32 setIndex, ResourceSetPipelineCompatability compatability, const ResourceSetCreateInfo& createInfo)
    {
        if (setIndex >= SetData.size() || !SetData[setIndex].Exists)
        {
            FL_ASSERT(false, "AllocateDescriptorSet invalid set index");
            return nullptr;
        }

        auto& data = SetData[setIndex];
        return std::make_shared<ResourceSet>(createInfo, compatability, data.Pool);
    }

    bool PipelineDescriptorData::operator==(const PipelineDescriptorData& other) const
    {
        bool earlyChecks = SetData.size() == other.SetData.size() &&
                           TotalDynamicOffsets == other.TotalDynamicOffsets &&
                           Compatability == other.Compatability;
        if (!earlyChecks)
            return false;

        for (u32 i = 0; i < SetData.size(); i++)
        {
            auto& l = SetData[i];
            auto& r = other.SetData[i];
            if (l.Exists != r.Exists ||
                l.DynamicOffsetCount != r.DynamicOffsetCount ||
                !l.Pool->CheckCompatibility(r.Pool.get()))
                return false;
        }

        return true;
    }

    void PipelineDescriptorData::Initialize()
    {
        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;

        if(!FL_VK_CHECK_RESULT(vkCreateDescriptorSetLayout(
            Context::Devices().Device(),
            &layoutInfo,
            nullptr,
            &EmptySetLayout
        ), "Create empty layout"))
            throw std::exception();
    }

    void PipelineDescriptorData::Shutdown()
    {
        auto layout = EmptySetLayout;
        Context::FinalizerQueue().Push([=]()
        {
            vkDestroyDescriptorSetLayout(Context::Devices().Device(), layout, nullptr);
        }, "Free empty layout");
    }

    void PipelineSpecializationHelper::Populate(Shader** shaders, std::vector<SpecializationConstant>* specs, u32 count)
    {
        // Gather initial sizes 
        u32 dataSize = 0;
        u32 mapSize = 0;
        for (u32 i = 0; i < count; i++)
        {
            dataSize += shaders[i]->GetTotalSpecializationSize();
            mapSize += specs[i].size();
        }
        SpecData.resize(dataSize);
        MapEntries.reserve(mapSize);

        FL_ASSERT(
            mapSize == 0 || dataSize > 0,
            "Attempting to specialize constants but shader has none"
        );

        if (mapSize > 0)
        {
            FL_LOG_WARN(
                "Specializing %d constants with a total size of %d",
                mapSize,
                dataSize
            );
        }

        u32 dataOffset = 0;
        for (u32 i = 0; i < count; i++)
        {
            auto& specInfo = SpecInfos.emplace_back();
            specInfo.mapEntryCount = specs[i].size();
            specInfo.pMapEntries = MapEntries.data() + MapEntries.size();
            specInfo.dataSize = dataSize;
            specInfo.pData = SpecData.data();
            for (auto& spec : specs[i])
            {
                FL_ASSERT(
                    shaders[i]->CheckSpecializationCompatability(spec),
                    "Passing specialization incompatible with shader to pipeline"
                );

                auto& mapEntry = MapEntries.emplace_back();
                mapEntry.constantID = spec.ConstantId;
                mapEntry.offset = dataOffset;
                mapEntry.size = 4; // All supported types are 4 bytes
                
                memcpy(SpecData.data() + dataOffset, &spec.Data, mapEntry.size);
                dataOffset += mapEntry.size;
            }
        }
    }

    void DescriptorBinder::Reset()
    {
        m_BoundData = nullptr;
    }

    void DescriptorBinder::BindResourceSet(const ResourceSet* set, u32 setIndex)
    {
        FL_PROFILE_FUNCTION();

        FL_CRASH_ASSERT(setIndex < m_BoundSets.size(), "Set index out of range");
        FL_CRASH_ASSERT(m_BoundData->SetData[setIndex].Exists, "Set index does not exist in the shader");
        FL_CRASH_ASSERT(set->GetPipelineCompatability() & m_BoundData->Compatability, "Set is not compatible with this pipeline type");

        #ifdef FL_DEBUG
        FL_ASSERT(
            m_BoundData->SetData[setIndex].Pool->CheckCompatibility(set->GetParentPool()),
            "Trying to bind incompatible resource set"
        );
        #endif
        
        m_BoundSets[setIndex] = set;
    }

    const ResourceSet* DescriptorBinder::GetResourceSet(u32 setIndex)
    {
        FL_PROFILE_FUNCTION();

        FL_CRASH_ASSERT(setIndex < m_BoundSets.size(), "Set index out of range");
        FL_CRASH_ASSERT(m_BoundData->SetData[setIndex].Exists, "Set index does not exist in the shader");

        return m_BoundSets[setIndex];
    }

    void DescriptorBinder::UpdateDynamicOffset(u32 setIndex, u32 bindingIndex, u32 offset)
    {
        FL_PROFILE_FUNCTION();
        
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
        FL_PROFILE_FUNCTION();

        if (!m_DynamicOffsets.empty())
            memset(m_DynamicOffsets.data(), 0, m_DynamicOffsets.size() * sizeof(u32));
        m_DynamicOffsets.resize(data->TotalDynamicOffsets);

        if (!m_BoundSets.empty())
            memset(m_BoundSets.data(), 0, m_BoundSets.size() * sizeof(void*));
        m_BoundSets.resize(data->SetData.size());

        m_BoundData = data;
    }
}
