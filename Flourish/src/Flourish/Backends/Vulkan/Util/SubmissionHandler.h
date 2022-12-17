#pragma once

#include "Flourish/Api/CommandBuffer.h"
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
    };

    class SubmissionHandler
    {
    public:
        void Initialize();
        void Shutdown();

        void WaitOnFrameSemaphores();
        void ProcessFrameSubmissions();
        
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
        
    private:
        
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