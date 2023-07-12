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
            const VkSemaphore* semaphores = nullptr,
            const u64* values = nullptr,
            u32 semaphoreCount = 0
        )
            : Lifetime(lifetime), Execute(execute), DebugName(debugName)
        {
            WaitSemaphores.assign(semaphores, semaphores + semaphoreCount);
            WaitValues.assign(values, values + semaphoreCount);
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
            const VkSemaphore* semaphores = nullptr,
            const u64* waitValues = nullptr,
            u32 semaphoreCount = 0,
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
