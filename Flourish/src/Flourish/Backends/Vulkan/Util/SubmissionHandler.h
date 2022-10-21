#pragma once

#include "Flourish/Backends/Vulkan/Util/Common.h"

namespace Flourish::Vulkan
{
    class RenderContext;
    class SubmissionHandler
    {
    public:
        void Initialize();
        void Shutdown();

        void ProcessSubmissions();

        // TS
        void PresentRenderContext(const RenderContext* context, int dependencySubmissionId);
        
    private:
        struct ProcessedSubmissionInfo
        {
            ProcessedSubmissionInfo(u32 startIndex, u32 count)
                : CompletionSemaphoresStartIndex(startIndex),
                  CompletionSemaphoresCount(count)
            {}

            u32 CompletionSemaphoresStartIndex;
            u32 CompletionSemaphoresCount;
        };
        
        struct RenderContextSubmission
        {
            RenderContextSubmission(const RenderContext* context, int dependencySubmissionId)
                : Context(context),
                  DependencySubmissionId(dependencySubmissionId)
            {}

            const RenderContext* Context;
            int DependencySubmissionId;
        };
        
        struct SemaphorePool
        {
            std::vector<VkSemaphore> Semaphores;
            u32 FreeIndex;
        };

    private:
        VkSemaphore GetTimelineSemaphore();
        VkSemaphore GetSemaphore();

    private:
        std::array<SemaphorePool, Flourish::Context::MaxFrameBufferCount> m_TimelineSemaphorePools;
        std::array<SemaphorePool, Flourish::Context::MaxFrameBufferCount> m_SemaphorePools;
        std::vector<RenderContextSubmission> m_PresentingContexts;
        std::mutex m_PresentingContextsLock;
    };
}