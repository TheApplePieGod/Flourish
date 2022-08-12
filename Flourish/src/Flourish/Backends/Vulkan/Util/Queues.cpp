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

        for (u32 i = 0; i < Flourish::Context::GetFrameBufferCount(); i++)
        {
            vkGetDeviceQueue(Context::Devices().Device(), m_PresentQueue.QueueIndex, std::min(i, indices.PresentQueueCount - 1), &m_PresentQueue.Queues[i]);
            vkGetDeviceQueue(Context::Devices().Device(), m_GraphicsQueue.QueueIndex, std::min(i, indices.GraphicsQueueCount - 1), &m_GraphicsQueue.Queues[i]);
            vkGetDeviceQueue(Context::Devices().Device(), m_ComputeQueue.QueueIndex, std::min(i, indices.ComputeQueueCount - 1), &m_ComputeQueue.Queues[i]);
            vkGetDeviceQueue(Context::Devices().Device(), m_TransferQueue.QueueIndex, std::min(i, indices.TransferQueueCount - 1), &m_TransferQueue.Queues[i]);
        }
    }

    void Queues::Shutdown()
    {
        
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
        std::vector<VkCommandBuffer> buffers;
        std::vector<VkSemaphore> semaphores;
        auto& queueData = GetQueueData(workloadType);
        queueData.CommandQueueLock.lock();
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
                buffers.push_back(value.Buffer);
                semaphores.push_back(value.WaitSemaphore);
                value.Submitted = true;
            }
        }
        queueData.CommandQueueLock.unlock();

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.signalSemaphoreCount = semaphores.size();
        submitInfo.pSignalSemaphores = semaphores.data();
        submitInfo.commandBufferCount = buffers.size();
        submitInfo.pCommandBuffers = buffers.data();

        FL_VK_ENSURE_RESULT(vkQueueSubmit(Context::Queues().TransferQueue(), 1, &submitInfo, nullptr));
    }

    VkQueue Queues::GraphicsQueue() const
    {
        return m_GraphicsQueue.Queues[Context::FrameIndex()];
    }

    VkQueue Queues::PresentQueue() const
    {
        return m_PresentQueue.Queues[Context::FrameIndex()];
    }

    VkQueue Queues::ComputeQueue() const
    {
        return m_ComputeQueue.Queues[Context::FrameIndex()];
    }

    VkQueue Queues::TransferQueue() const
    {
        return m_TransferQueue.Queues[Context::FrameIndex()];
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

        FL_ASSERT(false, "Command pool for workload not supported");
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