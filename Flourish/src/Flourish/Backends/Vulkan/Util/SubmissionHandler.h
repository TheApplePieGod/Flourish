#pragma once

#include "Flourish/Api/RenderGraph.h"
#include "Flourish/Backends/Vulkan/Util/Common.h"
#include "Flourish/Backends/Vulkan/Util/Synchronization.h"

namespace Flourish::Vulkan
{
    struct CommandSubmissionData
    {
        std::vector<VkSubmitInfo> GraphicsSubmitInfos;
        std::vector<VkSubmitInfo> ComputeSubmitInfos;
        std::vector<VkSubmitInfo> TransferSubmitInfos;
        std::vector<VkSemaphore> CompletionSemaphores;
        std::vector<u64> CompletionSemaphoreValues;
        std::vector<VkPipelineStageFlags> CompletionWaitStages;

        std::vector<VkSubmitInfo> SubmitInfos;
        std::vector<VkTimelineSemaphoreSubmitInfo> TimelineSubmitInfos;
        std::vector<u64> SyncSemaphoreValues;
        std::array<VkSemaphore, Flourish::Context::MaxFrameBufferCount> SyncSemaphores;
        VkPipelineStageFlags DrawWaitStages[1] = { VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT };
        VkPipelineStageFlags TransferWaitStages[1] = { VK_PIPELINE_STAGE_TRANSFER_BIT };
        VkPipelineStageFlags ComputeWaitStages[1] = { VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT };
    };

    class SubmissionHandler
    {
    public:
        void Initialize();
        void Shutdown();

        void WaitOnFrameSemaphores();
        void ProcessFrameSubmissions(const std::vector<std::vector<Flourish::CommandBuffer*>>& buffers, bool submit);
        void ProcessFrameSubmissions2(Flourish::CommandBuffer* const* buffers, u32 bufferCount, bool submit);
        
        // TS
        void ProcessPushSubmission(const std::vector<std::vector<Flourish::CommandBuffer*>>& buffers, std::function<void()> callback = nullptr);
        void ProcessExecuteSubmission(const std::vector<std::vector<Flourish::CommandBuffer*>>& buffers);
        
    public:
        static void ProcessSubmission(
            CommandSubmissionData& submissionData,
            const std::vector<std::vector<Flourish::CommandBuffer*>>& buffers,
            const std::vector<Flourish::RenderContext*>* contexts,
            std::vector<VkSemaphore>* finalSemaphores = nullptr,
            std::vector<u64>* finalSemaphoreValues = nullptr
        );
        static void ProcessSubmission2(
            CommandSubmissionData& submissionData,
            const std::vector<Flourish::RenderGraph*>& graphs,
            std::vector<VkSemaphore>* finalSemaphores = nullptr,
            std::vector<u64>* finalSemaphoreValues = nullptr
        );
        
    private:
        std::array<std::vector<VkSemaphore>, Flourish::Context::MaxFrameBufferCount> m_FrameWaitSemaphores;
        std::array<std::vector<u64>, Flourish::Context::MaxFrameBufferCount> m_FrameWaitSemaphoreValues;
        CommandSubmissionData m_FrameSubmissionData;
        CommandSubmissionData m_PushSubmissionData;
        CommandSubmissionData m_ExecuteSubmissionData;
        std::mutex m_PushDataMutex;
        std::mutex m_ExecuteDataMutex;
    };
}
