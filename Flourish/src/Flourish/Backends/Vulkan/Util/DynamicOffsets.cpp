#include "flpch.h"
#include "DynamicOffsets.h"

#include "Flourish/Backends/Vulkan/Shader.h"
#include "Flourish/Backends/Vulkan/Util/DescriptorPool.h"

namespace Flourish::Vulkan
{
    // TODO: Validation: descriptor set write, then dynamic offset update, then bind
    void DynamicOffsets::UpdateOffset(const Shader* shader, u32 setIndex, u32 bindingIndex, u32 offset)
    {
        const auto& data = shader->GetSetData(setIndex);
        
        // WHAT???????
        if (!data.Pool->DoesBindingExist(bindingIndex))
        {
            FL_LOG_ERROR("Attempting to update dynamic offset for descriptor binding %d that doesn't exist in the set", bindingIndex);
            throw std::exception();
        }
        
        // TODO: validate total offset ???
        m_DynamicOffsets[
            pool.OffsetIndex +
            pool.Pool->GetBindingData()[bindingIndex].OffsetIndex
        ] = offset;
    }

    // TODO: set bounds checking
    u32* DynamicOffsets::GetOffsetData(const Shader* shader, u32 setIndex)
    {
        return m_DynamicOffsets.data() + shader->GetSetData(setIndex).OffsetIndex;
    }

    void DynamicOffsets::ResetOffsets(const Shader* shader)
    {
        if (!m_DynamicOffsets.empty())
            memset(m_DynamicOffsets.data(), 0, m_DynamicOffsets.size() * sizeof(u32));
        m_DynamicOffsets.resize(shader->GetTotalDynamicOffsets());
    }
}
