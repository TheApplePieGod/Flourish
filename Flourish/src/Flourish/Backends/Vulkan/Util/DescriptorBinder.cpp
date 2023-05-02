#include "flpch.h"
#include "DescriptorBinder.h"

#include "Flourish/Backends/Vulkan/Shader.h"
#include "Flourish/Backends/Vulkan/DescriptorSet.h"
#include "Flourish/Backends/Vulkan/Util/DescriptorPool.h"

namespace Flourish::Vulkan
{
    void DescriptorBinder::Reset()
    {
        m_BoundShader = nullptr;
    }

    void DescriptorBinder::BindDescriptorSet(const DescriptorSet* set, u32 setIndex)
    {
        FL_CRASH_ASSERT(setIndex < m_BoundSets.size(), "Set index out of range");
        FL_CRASH_ASSERT(m_BoundShader->GetSetData()[setIndex].Exists, "Set index does not exist in the shader");

        #ifdef FL_DEBUG
        FL_ASSERT(
            m_BoundShader->GetSetData()[setIndex].Pool->CheckCompatibility(set->GetParentPool()),
            "Trying to bind incompatible descriptor set"
        );
        #endif
        
        m_BoundSets[setIndex] = set;
    }

    const DescriptorSet* DescriptorBinder::GetDescriptorSet(u32 setIndex)
    {
        FL_CRASH_ASSERT(setIndex < m_BoundSets.size(), "Set index out of range");
        FL_CRASH_ASSERT(m_BoundShader->GetSetData()[setIndex].Exists, "Set index does not exist in the shader");

        return m_BoundSets[setIndex];
    }

    void DescriptorBinder::UpdateDynamicOffset(u32 setIndex, u32 bindingIndex, u32 offset)
    {
        FL_CRASH_ASSERT(setIndex < m_BoundSets.size(), "Set index out of range");
        FL_CRASH_ASSERT(m_BoundSets[setIndex], "Must bind set before calling UpdateDynamicOffset");

        const auto& data = m_BoundShader->GetSetData()[setIndex];
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

        return m_DynamicOffsets.data() + m_BoundShader->GetSetData()[setIndex].DynamicOffsetIndex;
    }

    void DescriptorBinder::BindNewShader(const Shader* shader)
    {
        if (!m_DynamicOffsets.empty())
            memset(m_DynamicOffsets.data(), 0, m_DynamicOffsets.size() * sizeof(u32));
        m_DynamicOffsets.resize(shader->GetTotalDynamicOffsets());

        if (!m_BoundSets.empty())
            memset(m_BoundSets.data(), 0, m_BoundSets.size() * sizeof(void*));
        m_BoundSets.resize(shader->GetSetData().size());

        m_BoundShader = shader;
    }
}
