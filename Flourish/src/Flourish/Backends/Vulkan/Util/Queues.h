#pragma once

#include "Flourish/Backends/Vulkan/Util/Common.h"

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

    struct QueueCommandEntry
    {
        VkCommandBuffer Buffer;
        std::function<void()> CompletionCallback;
    };

    class Queues
    {
    public:
        void Initialize();
        void Shutdown();

        // TS
        void PushTransferCommand(const QueueCommandEntry& entry);
        void IterateTransferCommands();

        // TS
        inline VkQueue PresentQueue(u32 frameIndex) const { return m_PresentQueues[frameIndex]; }
        inline VkQueue GraphicsQueue(u32 frameIndex) const { return m_GraphicsQueues[frameIndex]; }
        inline VkQueue ComputeQueue(u32 frameIndex) const { return m_ComputeQueues[frameIndex]; }
        inline VkQueue TransferQueue(u32 frameIndex) const { return m_TransferQueues[frameIndex]; }
        inline u32 GraphicsQueueIndex() const { return m_GraphicsQueueIndex; }
        inline u32 PresentQueueIndex() const { return m_PresentQueueIndex; }
        inline u32 ComputeQueueIndex() const { return m_ComputeQueueIndex; }
        inline u32 TransferQueueIndex() const { return m_TransferQueueIndex; }

    public:
        // TS
        static QueueFamilyIndices GetQueueFamilies(VkPhysicalDevice device);

    private:
        std::array<VkQueue, Flourish::Context::MaxFrameBufferCount> m_GraphicsQueues, m_ComputeQueues, m_TransferQueues, m_PresentQueues;
        u32 m_GraphicsQueueIndex, m_PresentQueueIndex, m_ComputeQueueIndex, m_TransferQueueIndex;
        std::deque<QueueCommandEntry> m_TransferCommandQueue;
        std::mutex m_TransferCommandQueueLock;
    };
}