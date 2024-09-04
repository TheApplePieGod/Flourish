#include "flpch.h"
#include "Queues.h"

#include "Flourish/Backends/Vulkan/Context.h"
#include "Flourish/Backends/Vulkan/Util/Synchronization.h"

namespace Flourish::Vulkan
{
    void Queues::Initialize()
    {
        FL_LOG_TRACE("Vulkan queues initialization begin");

        auto indices = GetQueueFamilies(Context::Devices().PhysicalDevice());

        std::unordered_map<u32, u32> uniqueFamilies;
        uniqueFamilies.insert({ indices.GraphicsFamily.value(), indices.GraphicsQueueCount });
        uniqueFamilies.insert({ indices.ComputeFamily.value(), indices.ComputeQueueCount });
        uniqueFamilies.insert({ indices.TransferFamily.value(), indices.TransferQueueCount });
        uniqueFamilies.insert({ indices.PresentFamily.value(), indices.PresentQueueCount });

        if (indices.PresentFamily.value() != indices.GraphicsFamily.value())
        {
            FL_LOG_WARN("Present queue and graphics queue are different"); 
        }

        u32 physicalIdx = 0;
        for (auto& fam : uniqueFamilies)
        {
            auto& physical = m_PhysicalQueues[physicalIdx];
            for (u32 i = 0; i < Flourish::Context::FrameBufferCount(); i++)
            {
                vkGetDeviceQueue(
                    Context::Devices().Device(),
                    fam.first,
                    i % fam.second,
                    &physical.Queues[i]
                );
            }

            physical.QueueIndex = fam.first;

            if (indices.GraphicsFamily.value() == fam.first)
                m_VirtualQueues[static_cast<u32>(GPUWorkloadType::Graphics)] = physicalIdx;
            if (indices.ComputeFamily.value() == fam.first)
                m_VirtualQueues[static_cast<u32>(GPUWorkloadType::Compute)] = physicalIdx;
            if (indices.TransferFamily.value() == fam.first)
                m_VirtualQueues[static_cast<u32>(GPUWorkloadType::Transfer)] = physicalIdx;
            if (indices.PresentFamily.value() == fam.first)
                m_PresentQueue = physicalIdx;

            physicalIdx++;
        }

        FL_LOG_DEBUG("Graphics workloads assigned to queue %d", m_VirtualQueues[static_cast<u32>(GPUWorkloadType::Graphics)]);
        FL_LOG_DEBUG("Compute workloads assigned to queue %d", m_VirtualQueues[static_cast<u32>(GPUWorkloadType::Compute)]);
        FL_LOG_DEBUG("Transfer workloads assigned to queue %d", m_VirtualQueues[static_cast<u32>(GPUWorkloadType::Transfer)]);
    }

    void Queues::Shutdown()
    {
        FL_LOG_TRACE("Vulkan queues shutdown begin");

        for (auto fence : m_UnusedFences)
            vkDestroyFence(Context::Devices().Device(), fence, nullptr);
    }
    
    VkFence Queues::PushCommand(GPUWorkloadType workloadType, VkCommandBuffer buffer, std::function<void()> completionCallback, const char* debugName)
    {
        VkFence fence = RetrieveFence();

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &buffer;

        Synchronization::ResetFences(&fence, 1);

        LockQueue(workloadType, true);
        FL_VK_ENSURE_RESULT(vkQueueSubmit(Queue(workloadType), 1, &submitInfo, fence), "PushCommand queue submit");
        LockQueue(workloadType, false);

        Context::FinalizerQueue().PushAsync([this, completionCallback, fence]()
        {
            m_FencesLock.lock();
            m_UnusedFences.push_back(fence);
            m_FencesLock.unlock();

            if (completionCallback)
                completionCallback();
        }, &fence, 1, debugName);
        
        return fence;
    }

    void Queues::ExecuteCommand(GPUWorkloadType workloadType, VkCommandBuffer buffer, const char* debugName)
    {
        VkFence pushFence = PushCommand(workloadType, buffer, nullptr, debugName);
        
        Synchronization::WaitForFences(&pushFence, 1);
    }

    VkQueue Queues::PresentQueue() const
    {
        return m_PhysicalQueues[m_PresentQueue].Queues[Flourish::Context::FrameIndex()];
    }

    VkQueue Queues::Queue(GPUWorkloadType workloadType, u32 frameIndex) const
    {
        return GetQueueData(workloadType).Queues[frameIndex];
    }

    void Queues::LockQueue(GPUWorkloadType workloadType, bool lock)
    {
        std::mutex* mutex = &GetQueueData(workloadType).AccessMutex;
        if (lock)
            mutex->lock();
        else
            mutex->unlock();
    }

    void Queues::LockPresentQueue(bool lock)
    {
        std::mutex* mutex = &m_PhysicalQueues[m_PresentQueue].AccessMutex;
        if (lock)
            mutex->lock();
        else
            mutex->unlock();
    }
    
    u32 Queues::QueueIndex(GPUWorkloadType workloadType) const
    {
        return GetQueueData(workloadType).QueueIndex;
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

            // All queues support transfer
            indices.TransferFamily = i;
            indices.TransferQueueCount = family.queueCount;

            VkBool32 presentSupport = false;
            #ifdef FL_PLATFORM_WINDOWS
                presentSupport = vkGetPhysicalDeviceWin32PresentationSupportKHR(device, i);
            #elif defined(FL_PLATFORM_LINUX)
                // TODO: we need to figure out where these params come from
                // presentSupport = vkGetPhysicalDeviceXcbPresentationSupportKHR(device, i, ???, ???);
                presentSupport = true;
            #else
                // Doesn't seem to be any function to verify, so it should always be true
                presentSupport = family.queueFlags & VK_QUEUE_GRAPHICS_BIT;
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
        return m_PhysicalQueues[m_VirtualQueues[static_cast<u32>(workloadType)]];
    }

    const Queues::QueueData& Queues::GetQueueData(GPUWorkloadType workloadType) const
    {
        return m_PhysicalQueues[m_VirtualQueues[static_cast<u32>(workloadType)]];
    }

    VkFence Queues::RetrieveFence()
    {
        m_FencesLock.lock();
        if (m_UnusedFences.size() > 0)
        {
            VkFence fence = m_UnusedFences.back();
            m_UnusedFences.pop_back();
            m_FencesLock.unlock();
            return fence;
        }
        m_FencesLock.unlock();

        return Synchronization::CreateFence();
    }
}
