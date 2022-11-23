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
        
        struct SemaphoreWaitInfo
        {
            SemaphoreWaitInfo(VkSemaphore sem, u64 val)
                : Semaphore(sem), WaitValue(val)
            {}

            VkSemaphore Semaphore;
            u64 WaitValue;
        };

    private:
        std::vector<const RenderContext*> m_PresentingContexts;
        std::array<std::vector<SemaphoreWaitInfo>, Flourish::Context::MaxFrameBufferCount> m_FrameWaitSemaphores;
        SubmissionData m_SubmissionData;
        std::mutex m_PresentingContextsLock;
    };
}