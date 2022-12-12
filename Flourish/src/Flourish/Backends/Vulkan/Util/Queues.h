#pragma once

#include "Flourish/Backends/Vulkan/Util/Common.h"
#include "Flourish/Api/CommandBuffer.h"

namespace Flourish::Vulkan
{
    struct QueueFamilyIndices
    {
        std::optional<u32> GraphicsFamily;
        std::optional<u32> PresentFamily;
        std::optional<u32> ComputeFamily;
        std::optional<u32> TransferFamily;
        
        u32 GraphicsQueueCount;
        u32 PresentQueueCount;
        u32 ComputeQueueCount;
        u32 TransferQueueCount;

        bool IsComplete()
        {
            return (
                GraphicsFamily.has_value() &&
                PresentFamily.has_value() &&
                ComputeFamily.has_value() &&
                TransferFamily.has_value()
            );
        }
    };

    struct PushCommandResult
    {
        VkSemaphore SignalSemaphore;
        u64 SignalValue;
    };

    class Queues
    {
    public:
        void Initialize();
        void Shutdown();

        // TS
        PushCommandResult PushCommand(
            GPUWorkloadType workloadType,
            VkCommandBuffer buffer,
            std::function<void()> completionCallback = nullptr,
            const char* debugName = nullptr
        );
        void ExecuteCommand(GPUWorkloadType workloadType, VkCommandBuffer buffer, const char* debugName = nullptr);

        // TS
        VkQueue PresentQueue() const;
        VkQueue Queue(GPUWorkloadType workloadType) const;
        void LockQueue(GPUWorkloadType workloadType, bool lock);
        inline u32 PresentQueueIndex() const { return m_PresentQueue.QueueIndex; }
        u32 QueueIndex(GPUWorkloadType workloadType) const;

    public:
        // TS
        static QueueFamilyIndices GetQueueFamilies(VkPhysicalDevice device);

    private:
        struct QueueCommandEntry
        {
            QueueCommandEntry(VkCommandBuffer buffer, std::function<void()> callback, VkSemaphore semaphore, u64 val)
                : Buffer(buffer), Callback(callback), WaitSemaphore(semaphore), SignalValue(val)
            {}

            VkCommandBuffer Buffer;
            std::function<void()> Callback;
            VkSemaphore WaitSemaphore;
            u64 SignalValue;
            bool Submitted = false;
        };

        struct QueueData
        {
            std::array<VkQueue, Flourish::Context::MaxFrameBufferCount> Queues;
            u32 QueueIndex;
            std::mutex AccessMutex;
        };

    private:
        QueueData& GetQueueData(GPUWorkloadType workloadType);
        VkSemaphore RetrieveSemaphore();

    private:
        QueueData m_GraphicsQueue, m_ComputeQueue, m_TransferQueue, m_PresentQueue;
        std::vector<VkSemaphore> m_UnusedSemaphores;
        std::mutex m_SemaphoresLock;
        u64 m_ExecuteSemaphoreSignalValue = 0;
    };
}