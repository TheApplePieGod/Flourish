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
            const VkFence* fences = nullptr,
            u32 fenceCount = 0
        )
            : Lifetime(lifetime), Execute(execute), DebugName(debugName)
        {
            WaitFences.assign(fences, fences + fenceCount);
        }

        u32 Lifetime = 0; // Frames
        std::function<void()> Execute;
        const char* DebugName;
        std::vector<VkFence> WaitFences;
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
            const VkFence* fences = nullptr,
            u32 fenceCount = 0,
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
