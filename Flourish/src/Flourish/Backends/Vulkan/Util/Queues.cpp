#include "flpch.h"
#include "Queues.h"

#include "Flourish/Backends/Vulkan/Util/Context.h"
#include "Flourish/Backends/Vulkan/Util/Semaphore.h"

namespace Flourish::Vulkan
{
    void Queues::Initialize()
    {
        auto indices = GetQueueFamilies(Context::Devices().PhysicalDevice());

        m_GraphicsQueue.QueueIndex = indices.GraphicsFamily.value();
        m_PresentQueue.QueueIndex = indices.PresentFamily.value();
        m_ComputeQueue.QueueIndex = indices.ComputeFamily.value();
        m_TransferQueue.QueueIndex = indices.TransferFamily.value();

        for (u32 i = 0; i < Flourish::Context::FrameBufferCount(); i++)
        {
            vkGetDeviceQueue(Context::Devices().Device(), m_PresentQueue.QueueIndex, std::min(i, indices.PresentQueueCount - 1), &m_PresentQueue.Queues[i]);
            vkGetDeviceQueue(Context::Devices().Device(), m_GraphicsQueue.QueueIndex, std::min(i, indices.GraphicsQueueCount - 1), &m_GraphicsQueue.Queues[i]);
            vkGetDeviceQueue(Context::Devices().Device(), m_ComputeQueue.QueueIndex, std::min(i, indices.ComputeQueueCount - 1), &m_ComputeQueue.Queues[i]);
            vkGetDeviceQueue(Context::Devices().Device(), m_TransferQueue.QueueIndex, std::min(i, indices.TransferQueueCount - 1), &m_TransferQueue.Queues[i]);
        }
    }

    void Queues::Shutdown()
    {
        ClearCommands();

        auto semaphores = m_UnusedSemaphores;
        Context::DeleteQueue().Push([=]()
        {
            for (auto semaphore : semaphores)
                vkDestroySemaphore(Context::Devices().Device(), semaphore, nullptr);
        });
    }
    
    void Queues::PushCommand(GPUWorkloadType workloadType, VkCommandBuffer buffer, std::function<void()>&& completionCallback)
    {
        auto& queueData = GetQueueData(workloadType);
        queueData.CommandQueueLock.lock();
        queueData.CommandQueue.emplace_back(buffer, completionCallback, RetrieveSemaphore());
        queueData.CommandQueueLock.unlock();
    }

    void Queues::ExecuteCommand(GPUWorkloadType workloadType, VkCommandBuffer buffer)
    {

    }

    void Queues::IterateCommands(GPUWorkloadType workloadType)
    {
        auto& queueData = GetQueueData(workloadType);
        queueData.CommandQueueLock.lock();
        queueData.Buffers.clear();
        queueData.Semaphores.clear();
        queueData.SignalValues.clear();
        for (u32 i = 0; i < queueData.CommandQueue.size(); i++)
        {
            auto& value = queueData.CommandQueue.front();
            if (value.Submitted)
            {
                u64 semaphoreVal;
                vkGetSemaphoreCounterValue(Context::Devices().Device(), value.WaitSemaphore, &semaphoreVal);
                if (semaphoreVal > 0) // Completed
                {
                    value.Callback();
                    m_UnusedSemaphoresLock.lock();
                    m_UnusedSemaphores.push_back(value.WaitSemaphore);
                    m_UnusedSemaphoresLock.unlock();
                    queueData.CommandQueue.pop_front();
                    i--;
                }
            }
            else
            {
                queueData.Buffers.push_back(value.Buffer);
                queueData.Semaphores.push_back(value.WaitSemaphore);
                queueData.SignalValues.push_back(1);
                value.Submitted = true;
            }
        }
        queueData.CommandQueueLock.unlock();

        VkTimelineSemaphoreSubmitInfo timelineSubmitInfo{};
        timelineSubmitInfo.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
        timelineSubmitInfo.signalSemaphoreValueCount = queueData.SignalValues.size();
        timelineSubmitInfo.pSignalSemaphoreValues = queueData.SignalValues.data();

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.pNext = &timelineSubmitInfo;
        submitInfo.signalSemaphoreCount = queueData.Semaphores.size();
        submitInfo.pSignalSemaphores = queueData.Semaphores.data();
        submitInfo.commandBufferCount = queueData.Buffers.size();
        submitInfo.pCommandBuffers = queueData.Buffers.data();

        FL_VK_ENSURE_RESULT(vkQueueSubmit(Queue(workloadType), 1, &submitInfo, nullptr));
    }

    void Queues::IterateCommands()
    {
        IterateCommands(GPUWorkloadType::Graphics);
        IterateCommands(GPUWorkloadType::Compute);
        IterateCommands(GPUWorkloadType::Transfer);
    }

    void Queues::ClearCommands(GPUWorkloadType workloadType)
    {
        auto& queueData = GetQueueData(workloadType);
        queueData.CommandQueueLock.lock();
        for (u32 i = 0; i < queueData.CommandQueue.size(); i++)
        {
            auto& value = queueData.CommandQueue.front();
            value.Callback();
            m_UnusedSemaphoresLock.lock();
            m_UnusedSemaphores.push_back(value.WaitSemaphore);
            m_UnusedSemaphoresLock.unlock();
        }
        queueData.CommandQueueLock.unlock();
    }

    void Queues::ClearCommands()
    {
        ClearCommands(GPUWorkloadType::Graphics);
        ClearCommands(GPUWorkloadType::Compute);
        ClearCommands(GPUWorkloadType::Transfer);
    }

    VkQueue Queues::GraphicsQueue() const
    {
        return m_GraphicsQueue.Queues[Flourish::Context::FrameIndex()];
    }

    VkQueue Queues::PresentQueue() const
    {
        return m_PresentQueue.Queues[Flourish::Context::FrameIndex()];
    }

    VkQueue Queues::ComputeQueue() const
    {
        return m_ComputeQueue.Queues[Flourish::Context::FrameIndex()];
    }

    VkQueue Queues::TransferQueue() const
    {
        return m_TransferQueue.Queues[Flourish::Context::FrameIndex()];
    }

    VkQueue Queues::Queue(GPUWorkloadType workloadType) const
    {
        switch (workloadType)
        {
            case GPUWorkloadType::Graphics:
            { return GraphicsQueue(); }
            case GPUWorkloadType::Transfer:
            { return TransferQueue(); }
            case GPUWorkloadType::Compute:
            { return ComputeQueue(); }
        }

        FL_ASSERT(false, "Queue for workload not supported");
        return GraphicsQueue();
    }
    
    QueueFamilyIndices Queues::GetQueueFamilies(VkPhysicalDevice device)
    {
        QueueFamilyIndices indices;

        u32 supportedQueueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &supportedQueueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> supportedQueueFamilies(supportedQueueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &supportedQueueFamilyCount, supportedQueueFamilies.data());

        // Pass over the supported families and populate indices with the first available option
        int i = 0;
        for (const auto& family : supportedQueueFamilies)
        {
            if (indices.IsComplete()) break;

            if (family.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                indices.GraphicsFamily = i;
                indices.GraphicsQueueCount = family.queueCount;
            }

            if (family.queueFlags & VK_QUEUE_COMPUTE_BIT)
            {
                indices.ComputeFamily = i;
                indices.ComputeQueueCount = family.queueCount;
            }

            if (family.queueFlags & VK_QUEUE_TRANSFER_BIT)
            {
                indices.TransferFamily = i;
                indices.TransferQueueCount = family.queueCount;
            }

            VkBool32 presentSupport = false;
            #ifdef FL_PLATFORM_WINDOWS
                presentSupport = vkGetPhysicalDeviceWin32PresentationSupportKHR(device, i);
            #elif defined(FL_PLATFORM_LINUX)
                // TODO: we need to figure out where these params come from
                // presentSupport = vkGetPhysicalDeviceXcbPresentationSupportKHR(device, i, ???, ???);
                presentSupport = true;
            #endif

            if (presentSupport)
            {
                indices.PresentFamily = i;
                indices.PresentQueueCount = family.queueCount;
            }
            
            i++;
        }

        i = 0;
        for (const auto& family : supportedQueueFamilies)
        {
            // Prioritize families that support compute but not graphics
            if (family.queueFlags & VK_QUEUE_COMPUTE_BIT && !(family.queueFlags & VK_QUEUE_GRAPHICS_BIT))
            {
                indices.ComputeFamily = i;
                indices.ComputeQueueCount = family.queueCount;
            }

            // Prioritize families that support transfer only
            if (family.queueFlags & VK_QUEUE_TRANSFER_BIT && !(family.queueFlags & VK_QUEUE_GRAPHICS_BIT) && !(family.queueFlags & VK_QUEUE_COMPUTE_BIT))
            {
                indices.TransferFamily = i;
                indices.TransferQueueCount = family.queueCount;
            }

            i++;
        }

        return indices;
    }

    Queues::QueueData& Queues::GetQueueData(GPUWorkloadType workloadType)
    {
        switch (workloadType)
        {
            case GPUWorkloadType::Graphics:
            { return m_GraphicsQueue; }
            case GPUWorkloadType::Transfer:
            { return m_TransferQueue; }
            case GPUWorkloadType::Compute:
            { return m_ComputeQueue; }
        }

        FL_ASSERT(false, "Queue data for workload not supported");
        return m_GraphicsQueue;
    }

    VkSemaphore Queues::RetrieveSemaphore()
    {
        if (m_UnusedSemaphores.size() > 0)
        {
            m_UnusedSemaphoresLock.lock();
            auto semaphore = m_UnusedSemaphores.back();
            m_UnusedSemaphores.pop_back();
            m_UnusedSemaphoresLock.unlock();
            return semaphore;
        }

        return Semaphore::CreateTimelineSemaphore(0);
    }
}