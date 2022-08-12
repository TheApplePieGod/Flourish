#include "flpch.h"
#include "CommandBuffer.h"

#include "Flourish/Backends/Vulkan/Util/Context.h"

namespace Flourish::Vulkan
{
    CommandBuffer::CommandBuffer(const CommandBufferCreateInfo& createInfo)
        : Flourish::CommandBuffer(createInfo)
    {
        m_AllocatedThread = std::this_thread::get_id();
        Context::Commands().AllocateBuffers(
            m_WorkloadType,
            true,
            m_CommandBuffers.data(),
            Flourish::Context::FrameBufferCount(),
            m_AllocatedThread
        );   
    }

    CommandBuffer::~CommandBuffer()
    {
        FL_ASSERT(
            m_AllocatedThread == std::this_thread::get_id(),
            "Command buffer should never be destroyed from a thread different than the one that created it"
        );
        FL_ASSERT(
            Flourish::Context::IsThreadRegistered(),
            "Destroying command buffer on deregistered thread, which will cause a memory leak"
        );

        auto buffers = m_CommandBuffers;
        auto thread = m_AllocatedThread;
        auto workloadType = m_WorkloadType;
        Context::DeleteQueue().Push([=]()
        {
            Context::Commands().FreeBuffers(
                workloadType,
                buffers.data(),
                Flourish::Context::FrameBufferCount(),
                thread
            );
        });
    }
}