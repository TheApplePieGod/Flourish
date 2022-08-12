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

    class Queues
    {
    public:
        void Initialize();
        void Shutdown();

        // TS
        void PushCommand(GPUWorkloadType workloadType, VkCommandBuffer buffer, std::function<void()>&& completionCallback);
        void ExecuteCommand(GPUWorkloadType workloadType, VkCommandBuffer buffer);
        void IterateCommands(GPUWorkloadType workloadType);

        // TS
        VkQueue GraphicsQueue() const;
        VkQueue PresentQueue() const;
        VkQueue ComputeQueue() const;
        VkQueue TransferQueue() const;
        inline u32 GraphicsQueueIndex() const { return m_GraphicsQueue.QueueIndex; }
        inline u32 PresentQueueIndex() const { return m_PresentQueue.QueueIndex; }
        inline u32 ComputeQueueIndex() const { return m_ComputeQueue.QueueIndex; }
        inline u32 TransferQueueIndex() const { return m_TransferQueue.QueueIndex; }

    public:
        // TS
        static QueueFamilyIndices GetQueueFamilies(VkPhysicalDevice device);

    private:
        struct QueueCommandEntry
        {
            QueueCommandEntry(VkCommandBuffer buffer, std::function<void()> callback, VkSemaphore semaphore)
                : Buffer(buffer), Callback(callback), WaitSemaphore(semaphore)
            {}

            VkCommandBuffer Buffer;
            std::function<void()> Callback;
            VkSemaphore WaitSemaphore;
            bool Submitted = false;
        };

        struct QueueData
        {
            std::array<VkQueue, Flourish::Context::MaxFrameBufferCount> Queues;
            u32 QueueIndex;
            std::deque<QueueCommandEntry> CommandQueue;
            std::mutex CommandQueueLock;
        };

    private:
        QueueData& GetQueueData(GPUWorkloadType workloadType);
        VkSemaphore RetrieveSemaphore();

    private:
        QueueData m_GraphicsQueue, m_ComputeQueue, m_TransferQueue, m_PresentQueue;
        std::vector<VkSemaphore> m_UnusedSemaphores;
        std::mutex m_UnusedSemaphoresLock;
    };
}