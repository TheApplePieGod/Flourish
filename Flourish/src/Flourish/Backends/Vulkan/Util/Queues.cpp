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

        u32 physicalIdx = 0;
        std::unordered_map<u32, u32> uniqueFamilies;
        uniqueFamilies.insert({ indices.GraphicsFamily.value(), indices.GraphicsQueueCount });
        uniqueFamilies.insert({ indices.ComputeFamily.value(), indices.ComputeQueueCount });
        uniqueFamilies.insert({ indices.TransferFamily.value(), indices.TransferQueueCount });
        uniqueFamilies.insert({ indices.PresentFamily.value(), indices.PresentQueueCount });
        for (auto& fam : uniqueFamilies)
        {
            auto& physical = m_PhysicalQueues[physicalIdx];
            for (u32 i = 0; i < Flourish::Context::FrameBufferCount(); i++)
            {
                vkGetDeviceQueue(
                    Context::Devices().Device(),
                    fam.first,
                    std::min(i, fam.second - 1),
                    &physical.Queues[i]
                );
            }

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
    }

    void Queues::Shutdown()
    {
        FL_LOG_TRACE("Vulkan queues shutdown begin");

        for (auto semaphore : m_UnusedSemaphores)
            vkDestroySemaphore(Context::Devices().Device(), semaphore, nullptr);
    }
    
    PushCommandResult Queues::PushCommand(GPUWorkloadType workloadType, VkCommandBuffer buffer, std::function<void()> completionCallback, const char* debugName)
    {
        VkSemaphore semaphore = RetrieveSemaphore();

        m_SemaphoresLock.lock();
        u64 signalValue = ++m_ExecuteSemaphoreSignalValue;
        m_SemaphoresLock.unlock();

        VkTimelineSemaphoreSubmitInfo timelineSubmitInfo{};
        timelineSubmitInfo.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
        timelineSubmitInfo.signalSemaphoreValueCount = 1;
        timelineSubmitInfo.pSignalSemaphoreValues = &signalValue;

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.pNext = &timelineSubmitInfo;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &semaphore;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &buffer;

        LockQueue(workloadType, true);
        FL_VK_ENSURE_RESULT(vkQueueSubmit(Queue(workloadType), 1, &submitInfo, VK_NULL_HANDLE), "PushCommand queue submit");
        LockQueue(workloadType, false);

        Context::FinalizerQueue().PushAsync([this, completionCallback, semaphore]()
        {
            m_SemaphoresLock.lock();
            m_UnusedSemaphores.push_back(semaphore);
            m_SemaphoresLock.unlock();

            if (completionCallback)
                completionCallback();
        }, &semaphore, &signalValue, 1, debugName);
        
        return { semaphore, signalValue };
    }

    void Queues::ExecuteCommand(GPUWorkloadType workloadType, VkCommandBuffer buffer, const char* debugName)
    {
        auto pushResult = PushCommand(workloadType, buffer, nullptr, debugName);
        
        VkSemaphoreWaitInfo waitInfo{};
        waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
        waitInfo.semaphoreCount = 1;
        waitInfo.pSemaphores = &pushResult.SignalSemaphore;
        waitInfo.pValues = &pushResult.SignalValue;

        vkWaitSemaphoresKHR(Context::Devices().Device(), &waitInfo, UINT64_MAX);
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

    VkSemaphore Queues::RetrieveSemaphore()
    {
        m_SemaphoresLock.lock();
        if (m_UnusedSemaphores.size() > 0)
        {
            auto semaphore = m_UnusedSemaphores.back();
            m_UnusedSemaphores.pop_back();
            m_SemaphoresLock.unlock();
            return semaphore;
        }
        m_SemaphoresLock.unlock();

        return Synchronization::CreateTimelineSemaphore(0);
    }
}
