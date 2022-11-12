#include "flpch.h"
#include "Queues.h"

#include "Flourish/Backends/Vulkan/Util/Context.h"
#include "Flourish/Backends/Vulkan/Util/Synchronization.h"

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
            
            m_GraphicsQueue.Fences[i] = Synchronization::CreateFence();
            m_ComputeQueue.Fences[i] = Synchronization::CreateFence();
            m_TransferQueue.Fences[i] = Synchronization::CreateFence();
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
    
    void Queues::ResetQueueFence(GPUWorkloadType workloadType)
    {
        VkFence fence = nullptr;
        switch (workloadType)
        {
            case GPUWorkloadType::Graphics:
            { fence = m_GraphicsQueue.Fences[Flourish::Context::FrameIndex()]; } break;
            case GPUWorkloadType::Transfer:
            { fence = m_TransferQueue.Fences[Flourish::Context::FrameIndex()]; } break;
            case GPUWorkloadType::Compute:
            { fence = m_ComputeQueue.Fences[Flourish::Context::FrameIndex()]; } break;
        }

        vkResetFences(Context::Devices().Device(), 1, &fence);
    }
    
    void Queues::WaitForQueueFences()
    {
        VkFence fences[3] = {
            m_GraphicsQueue.Fences[Flourish::Context::FrameIndex()],
            m_ComputeQueue.Fences[Flourish::Context::FrameIndex()],
            m_TransferQueue.Fences[Flourish::Context::FrameIndex()]
        };

        vkWaitForFences(Context::Devices().Device(), 3, fences, VK_TRUE, UINT64_MAX);
    }
    
    void Queues::PushCommand(GPUWorkloadType workloadType, VkCommandBuffer buffer, std::function<void()> completionCallback)
    {
        auto& queueData = GetQueueData(workloadType);
        queueData.CommandQueueLock.lock();
        queueData.CommandQueue.emplace_back(buffer, completionCallback, RetrieveSemaphore());
        queueData.CommandQueueLock.unlock();
    }

    void Queues::ExecuteCommand(GPUWorkloadType workloadType, VkCommandBuffer buffer)
    {
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &buffer;

        FL_VK_ENSURE_RESULT(vkQueueSubmit(Queue(workloadType), 1, &submitInfo, nullptr));
    }

    void Queues::IterateCommands(GPUWorkloadType workloadType)
    {
        auto& queueData = GetQueueData(workloadType);
        if (queueData.CommandQueue.empty()) return;

        queueData.CommandQueueLock.lock();
        queueData.Buffers.clear();
        queueData.Semaphores.clear();
        queueData.SignalValues.clear();
        for (int i = 0; i < queueData.CommandQueue.size(); i++)
        {
            auto& value = queueData.CommandQueue.at(i);
            if (value.Submitted)
            {
                u64 semaphoreVal;
                vkGetSemaphoreCounterValueKHR(Context::Devices().Device(), value.WaitSemaphore, &semaphoreVal);
                if (semaphoreVal > 0) // Completed
                {
                    if (value.Callback) value.Callback();
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

        if (queueData.Buffers.empty()) return;
        
        VkTimelineSemaphoreSubmitInfo timelineSubmitInfo{};
        timelineSubmitInfo.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
        timelineSubmitInfo.signalSemaphoreValueCount = queueData.SignalValues.size();
        timelineSubmitInfo.pSignalSemaphoreValues = queueData.SignalValues.data();

        // Having implicit ording is a little silly so we should probably revisit this 
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
            auto& value = queueData.CommandQueue.at(i);
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

    VkQueue Queues::PresentQueue() const
    {
        return m_PresentQueue.Queues[Flourish::Context::FrameIndex()];
    }

    VkQueue Queues::Queue(GPUWorkloadType workloadType) const
    {
        switch (workloadType)
        {
            case GPUWorkloadType::Graphics:
            { return m_GraphicsQueue.Queues[Flourish::Context::FrameIndex()]; }
            case GPUWorkloadType::Transfer:
            { return m_TransferQueue.Queues[Flourish::Context::FrameIndex()]; }
            case GPUWorkloadType::Compute:
            { return m_ComputeQueue.Queues[Flourish::Context::FrameIndex()]; }
        }

        FL_ASSERT(false, "Queue for workload not supported");
        return nullptr;
    }
    
    u32 Queues::QueueIndex(GPUWorkloadType workloadType) const
    {
        switch (workloadType)
        {
            case GPUWorkloadType::Graphics:
            { return m_GraphicsQueue.QueueIndex; }
            case GPUWorkloadType::Transfer:
            { return m_TransferQueue.QueueIndex; }
            case GPUWorkloadType::Compute:
            { return m_ComputeQueue.QueueIndex; }
        }

        FL_ASSERT(false, "QueueIndex for workload not supported");
        return 0;
    }

    VkFence Queues::QueueFence(GPUWorkloadType workloadType) const
    {
        switch (workloadType)
        {
            case GPUWorkloadType::Graphics:
            { return m_GraphicsQueue.Fences[Flourish::Context::FrameIndex()]; }
            case GPUWorkloadType::Transfer:
            { return m_TransferQueue.Fences[Flourish::Context::FrameIndex()]; }
            case GPUWorkloadType::Compute:
            { return m_ComputeQueue.Fences[Flourish::Context::FrameIndex()]; }
        }

        FL_ASSERT(false, "QueueIndex for workload not supported");
        return 0;
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
            #else
                // Doesn't seem to be any function to verify, so it should always be true
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

        return Synchronization::CreateTimelineSemaphore(0);
    }
}