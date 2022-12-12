#include "flpch.h"
#include "FinalizerQueue.h"

#include "Flourish/Backends/Vulkan/Context.h"

namespace Flourish::Vulkan
{
    void FinalizerQueue::Initialize()
    {
        
    }

    void FinalizerQueue::Shutdown()
    {
        Iterate(true);
    }

    void FinalizerQueue::Push(std::function<void()> executeFunc, const char* debugName)
    {
        m_QueueLock.lock();
        m_Queue.emplace_back(
            Flourish::Context::FrameBufferCount() + 1,
            executeFunc,
            debugName
        );
        m_QueueLock.unlock();
    }

    void FinalizerQueue::PushAsync(
        std::function<void()> executeFunc,
        const std::vector<VkSemaphore>* semaphores,
        const std::vector<u64>* waitValues,
        const char* debugName)
    {
        m_QueueLock.lock();
        m_Queue.emplace_back(
            Flourish::Context::FrameBufferCount() + 1,
            executeFunc,
            debugName,
            semaphores,
            waitValues
        );
        m_QueueLock.unlock();
    }

    void FinalizerQueue::Iterate(bool force)
    {
        m_QueueLock.lock();
        for (u32 i = 0; i < m_Queue.size(); i++)
        {
            auto& value = m_Queue.at(i);
            
            bool execute = false;
            if (!value.WaitSemaphores.empty())
            {
                // Check all semaphores for completion
                u64 semaphoreVal;
                for (u32 i = 0; i < value.WaitSemaphores.size(); i++)
                {
                    vkGetSemaphoreCounterValueKHR(Context::Devices().Device(), value.WaitSemaphores[i], &semaphoreVal);
                    execute = semaphoreVal == value.WaitValues[i]; // Completed
                    if (!execute) break; // If any fail then all fail
                }
            }
            else if (value.Lifetime > 0)
                value.Lifetime -= 1;
            else
                execute = false;
            
            if (execute || force)
            {
                if (value.DebugName)
                { FL_LOG_TRACE("Finalizer: %s", value.DebugName); }
                m_QueueLock.unlock();
                value.Execute();
                m_QueueLock.lock();
                m_Queue.erase(m_Queue.begin() + i);
                i -= 1;
            }
        }
        m_QueueLock.unlock();
    }
}