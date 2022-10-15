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
        void PresentRenderContext(const RenderContext* context, u32 submissionId);
        
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
            RenderContextSubmission(const RenderContext* context, u32 submissionId)
                : Context(context),
                  SubmissionId(submissionId)
            {}

            const RenderContext* Context;
            u32 SubmissionId;
        };

    private:
        VkSemaphore GetSemaphore();

    private:
        u32 m_SemaphorePoolIndex = 0;
        std::array<std::vector<VkSemaphore>, Flourish::Context::MaxFrameBufferCount> m_SemaphorePools;
        std::vector<RenderContextSubmission> m_PresentingContexts;
        std::mutex m_PresentingContextsLock;
    };
}