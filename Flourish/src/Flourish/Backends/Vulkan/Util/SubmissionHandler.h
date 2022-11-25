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

        void WaitOnFrameSemaphores();
        void ProcessSubmissions();

        // TS
        void PresentRenderContext(RenderContext* context);
        
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
        std::vector<RenderContext*> m_PresentingContexts;
        std::array<std::vector<VkSemaphore>, Flourish::Context::MaxFrameBufferCount> m_FrameWaitSemaphores;
        std::array<std::vector<u64>, Flourish::Context::MaxFrameBufferCount> m_FrameWaitSemaphoreValues;
        SubmissionData m_SubmissionData;
        std::mutex m_PresentingContextsLock;
    };
}