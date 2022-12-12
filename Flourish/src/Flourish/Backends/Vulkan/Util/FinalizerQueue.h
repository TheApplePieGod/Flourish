#pragma once

#include "Flourish/Api/RenderContext.h"
#include "Flourish/Backends/Vulkan/Util/Common.h"

namespace Flourish::Vulkan
{
    struct DeleteEntry
    {
        DeleteEntry(
            u32 lifetime, 
            std::function<void()> execute,
            const char* debugName,
            const std::vector<VkSemaphore>* semaphores = nullptr,
            const std::vector<u64>* values = nullptr)
            : Lifetime(lifetime), Execute(execute), DebugName(debugName)
        {
            if (semaphores) WaitSemaphores = *semaphores;
            if (values) WaitValues = *values;
        }

        u32 Lifetime = 0; // Frames
        std::function<void()> Execute;
        const char* DebugName;
        std::vector<VkSemaphore> WaitSemaphores;
        std::vector<u64> WaitValues;
    };

    class FinalizerQueue
    {
    public:
        void Initialize();
        void Shutdown();

    public:
        // TS
        // Will run a delete operation after FrameCount() + 1 frames
        // Relies on work being synchronized by submission fences, so any async deletes must use
        // the other version of push
        void Push(std::function<void()> executeFunc, const char* debugName = nullptr);
        void PushAsync(
            std::function<void()> executeFunc,
            const std::vector<VkSemaphore>* semaphores = nullptr,
            const std::vector<u64>* waitValues = nullptr,
            const char* debugName = nullptr
        );
        void Iterate(bool force = false);
        
        // TS
        inline bool IsEmpty() const { return m_Queue.empty(); }

    private:
        std::deque<DeleteEntry> m_Queue;
        std::mutex m_QueueLock;
    };
}