#pragma once

#include "Flourish/Api/CommandBuffer.h"
#include "Flourish/Backends/Vulkan/Util/Common.h"
#include "Flourish/Backends/Vulkan/RenderCommandEncoder.h"

namespace Flourish::Vulkan
{
    class CommandBuffer : public Flourish::CommandBuffer
    {
    public:
        CommandBuffer(const CommandBufferCreateInfo& createInfo, bool secondary = false);
        ~CommandBuffer() override;

        void SubmitEncodedCommands(VkCommandBuffer buffer, GPUWorkloadType workloadType);
        Flourish::RenderCommandEncoder* EncodeRenderCommands(Flourish::Framebuffer* framebuffer) override;

        // TS
        inline const auto& GetEncoderSubmissions() const { return m_EncoderSubmissions; }

    private:
        struct EncoderSubmission
        {
            EncoderSubmission(VkCommandBuffer buffer, GPUWorkloadType workloadType)
                : Buffer(buffer), WorkloadType(workloadType)
            {}

            VkCommandBuffer Buffer;
            GPUWorkloadType WorkloadType;
        };
        
        struct SubmissionData
        {
            std::vector<VkSubmitInfo> GraphicsSubmitInfos;
            std::vector<VkSubmitInfo> ComputeSubmitInfos;
            std::vector<VkSubmitInfo> TransferSubmitInfos;
            std::vector<VkTimelineSemaphoreSubmitInfo> TimelineSubmitInfos;
            std::vector<u64> SyncSemaphoreValues;
            std::array<VkSemaphore, Flourish::Context::MaxFrameBufferCount> SyncSemaphores;
            VkPipelineStageFlags DrawWaitStages[1] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
            VkPipelineStageFlags TransferWaitStages[1] = { VK_PIPELINE_STAGE_TRANSFER_BIT };
            VkPipelineStageFlags ComputeWaitStages[1] = { VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT };
        };
        
    private:
        void CheckFrameUpdate();

    private:
        u64 m_LastFrameEncoding = 0;
        std::vector<RenderCommandEncoder> m_RenderCommandEncoderCache;
        u32 m_RenderCommandEncoderCachePtr = 0;
        std::vector<EncoderSubmission> m_EncoderSubmissions;
        SubmissionData m_SubmissionData;
        std::thread::id m_AllocatedThread;
    };
}