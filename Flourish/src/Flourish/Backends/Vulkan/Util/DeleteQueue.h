#pragma once

#include "Flourish/Api/RenderContext.h"
#include "Flourish/Backends/Vulkan/Util/Common.h"

namespace Flourish::Vulkan
{
    struct DeleteEntry
    {
        DeleteEntry(u32 lifetime, std::function<void()> execute)
            : Lifetime(lifetime), Execute(execute)
        {}

        u32 Lifetime = 0; // Frames
        std::function<void()> Execute;
    };

    class DeleteQueue
    {
    public:
        void Initialize();
        void Shutdown();

    public:
        // TS
        void Push(std::function<void()>&& executeFunc);
        void Iterate(bool force = false);
        
        // TS
        inline bool IsEmpty() const { return m_Queue.empty(); }

    private:
        std::deque<DeleteEntry> m_Queue;
        std::mutex m_QueueLock;
    };
}