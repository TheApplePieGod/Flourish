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

    void DeleteQueue::Push(std::function<void()>&& executeFunc)
    {
        m_QueueLock.lock();
        m_Queue.emplace_back(
            Flourish::Context::FrameBufferCount() + 1,
            executeFunc
        );
        m_QueueLock.unlock();
    }

    void DeleteQueue::Iterate(bool force)
    {
        m_QueueLock.lock();
        for (u32 i = 0; i < m_Queue.size(); i++)
        {
            auto& value = m_Queue.at(i);
            if (!force && value.Lifetime > 0)
                value.Lifetime -= 1;
            else
            {
                m_QueueLock.unlock();
                value.Execute();
                m_QueueLock.lock();
                m_Queue.pop_front();
                i -= 1;
            }
        }
        m_QueueLock.unlock();
    }
}