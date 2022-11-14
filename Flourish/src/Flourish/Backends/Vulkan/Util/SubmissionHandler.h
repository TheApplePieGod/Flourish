#pragma once

#include "Flourish/Backends/Vulkan/Util/Common.h"
#include "Flourish/Backends/Vulkan/Util/Synchronization.h"

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
        void PresentRenderContext(const RenderContext* context);
        
    private:
        struct SubmissionData
        {
            std::vector<VkSubmitInfo> GraphicsSubmitInfos;
            std::vector<VkSubmitInfo> ComputeSubmitInfos;
            std::vector<VkSubmitInfo> TransferSubmitInfos;
            std::vector<VkSemaphore> CompletionSemaphores;
            std::vector<u64> CompletionSemaphoreValues;
            std::vector<VkPipelineStageFlags> CompletionWaitStages;
        };

    private:
        std::vector<const RenderContext*> m_PresentingContexts;
        SubmissionData m_SubmissionData;
        std::mutex m_PresentingContextsLock;
    };
}