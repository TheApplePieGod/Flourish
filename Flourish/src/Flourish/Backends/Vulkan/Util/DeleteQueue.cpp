#include "flpch.h"
#include "DeleteQueue.h"

#include "Flourish/Backends/Vulkan/Util/Context.h"

namespace Flourish::Vulkan
{
    void DeleteQueue::Initialize()
    {
        
    }

    void DeleteQueue::Shutdown()
    {
        Iterate(true);
    }

    void DeleteQueue::Push(std::function<void()>&& executeFunc, const char* debugName)
    {
        m_QueueLock.lock();
        m_Queue.emplace_back(
            Flourish::Context::FrameBufferCount() + 1,
            executeFunc,
            debugName
        );
        m_QueueLock.unlock();
    }

    void DeleteQueue::PushAsync(std::function<void()>&& executeFunc, VkSemaphore semaphore, u64 waitValue, const char* debugName)
    {
        m_QueueLock.lock();
        m_Queue.emplace_back(
            Flourish::Context::FrameBufferCount() + 1,
            executeFunc,
            debugName,
            semaphore,
            waitValue
        );
        m_QueueLock.unlock();
    }

    void DeleteQueue::Iterate(bool force)
    {
        m_QueueLock.lock();
        for (u32 i = 0; i < m_Queue.size(); i++)
        {
            auto& value = m_Queue.at(i);
            
            bool execute = false;
            if (value.WaitSemaphore)
            {
                u64 semaphoreVal;
                vkGetSemaphoreCounterValueKHR(Context::Devices().Device(), value.WaitSemaphore, &semaphoreVal);
                execute = semaphoreVal == value.WaitValue; // Completed
            }
            else if (value.Lifetime > 0)
                value.Lifetime -= 1;
            else
                execute = false;
            
            if (execute || force)
            {
                if (value.DebugName)
                    FL_LOG_WARN("Delete queue: %s", value.DebugName);
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