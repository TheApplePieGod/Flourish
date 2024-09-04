#include "flpch.h"
#include "FinalizerQueue.h"

#include "Flourish/Backends/Vulkan/Context.h"

namespace Flourish::Vulkan
{
    void FinalizerQueue::Initialize()
    {
        FL_LOG_TRACE("Vulkan finalizer queue initialization begin");
    }

    void FinalizerQueue::Shutdown()
    {
        Iterate(true);
    }

    void FinalizerQueue::Push(std::function<void()> executeFunc, const char* debugName)
    {
        m_QueueLock.lock();
        m_Queue.emplace_back(
            Flourish::Context::FrameBufferCount() * 2 + 1,
            executeFunc,
            debugName
        );
        m_QueueLock.unlock();
    }

    void FinalizerQueue::PushAsync(
        std::function<void()> executeFunc,
        const VkFence* fences,
        u32 fenceCount,
        const char* debugName)
    {
        m_QueueLock.lock();
        m_Queue.emplace_back(
            Flourish::Context::FrameBufferCount() + 1,
            executeFunc,
            debugName,
            fences,
            fenceCount
        );
        m_QueueLock.unlock();
    }

    void FinalizerQueue::Iterate(bool force)
    {
        FL_PROFILE_FUNCTION();

        m_QueueLock.lock();
        for (int i = 0; i < m_Queue.size(); i++)
        {
            auto& value = m_Queue.at(i);
            
            bool execute = false;
            if (!value.WaitFences.empty())
            {
                // Check all fences for completion
                for (VkFence fence : value.WaitFences)
                {
                    execute = Synchronization::IsFenceSignalled(fence); // Completed
                    if (!execute) break; // If any fail then all fail
                }
            }
            else if (value.Lifetime > 0)
                value.Lifetime -= 1;
            else
                execute = true;
            
            if (execute || force)
            {
                if (value.DebugName)
                { FL_LOG_TRACE("Finalizer: %s", value.DebugName); }
                m_QueueLock.unlock();
                value.Execute();
                value.Execute = nullptr; // Ensure function data gets cleaned up before relocking
                m_QueueLock.lock();
                m_Queue.erase(m_Queue.begin() + i);
                i -= 1;
            }
        }
        m_QueueLock.unlock();
    }
}
