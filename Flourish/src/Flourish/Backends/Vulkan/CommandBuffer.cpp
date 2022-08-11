#include "flpch.h"
#include "CommandBuffer.h"

#include "Flourish/Backends/Vulkan/Util/Context.h"

namespace Flourish::Vulkan
{
    CommandBuffer::CommandBuffer(const CommandBufferCreateInfo& createInfo)
    {
        m_WorkloadType = createInfo.WorkloadType;
        Context::Commands().AllocateBuffers(
            m_WorkloadType,
            true,
            m_CommandBuffers.data(),
            Flourish::Context::GetFrameBufferCount()
        );   
    }

    CommandBuffer::~CommandBuffer()
    {
        
    }
}