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

        // Lock queues here to ensure we don't submit to the same queue on different threads concurrently. Additionally, lock the present queue
        // because it is likely that the present queue is just an alias for a different queue and is not its own thing
        LockPresentQueue(true);
        LockQueue(workloadType, true);
        FL_VK_ENSURE_RESULT(vkQueueSubmit(Queue(workloadType), 1, &submitInfo, nullptr), "PushCommand queue submit");
        LockQueue(workloadType, false);
        LockPresentQueue(false);

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

        vkWaitSemaphores(Context::Devices().Device(), &waitInfo, UINT64_MAX);
    }

    VkQueue Queues::PresentQueue() const
    {
        return m_PresentQueue.Queues[Flourish::Context::FrameIndex()];
    }

    VkQueue Queues::Queue(GPUWorkloadType workloadType, u32 frameIndex) const
    {
        switch (workloadType)
        {
            case GPUWorkloadType::Graphics:
            { return m_GraphicsQueue.Queues[frameIndex]; }
            case GPUWorkloadType::Transfer:
            { return m_TransferQueue.Queues[frameIndex]; }
            case GPUWorkloadType::Compute:
            { return m_ComputeQueue.Queues[frameIndex]; }
        }

        FL_ASSERT(false, "Queue for workload not supported");
        return nullptr;
    }

    void Queues::LockQueue(GPUWorkloadType workloadType, bool lock)
    {
        std::mutex* mutex;
        switch (workloadType)
        {
            case GPUWorkloadType::Graphics:
            { mutex = &m_GraphicsQueue.AccessMutex; }
            case GPUWorkloadType::Transfer:
            { mutex = &m_TransferQueue.AccessMutex; }
            case GPUWorkloadType::Compute:
            { mutex = &m_ComputeQueue.AccessMutex; }
        }

        FL_ASSERT(mutex, "Queue for workload not supported");

        if (lock)
            mutex->lock();
        else
            mutex->unlock();
    }

    void Queues::LockPresentQueue(bool lock)
    {
        if (lock)
            m_PresentQueue.AccessMutex.lock();
        else
            m_PresentQueue.AccessMutex.unlock();
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
