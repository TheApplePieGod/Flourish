#pragma once

#include "Flourish/Api/CommandBuffer.h"
#include "Flourish/Backends/Vulkan/Util/Common.h"
#include "Flourish/Backends/Vulkan/GraphicsCommandEncoder.h"
#include "Flourish/Backends/Vulkan/RenderCommandEncoder.h"
#include "Flourish/Backends/Vulkan/ComputeCommandEncoder.h"
#include "Flourish/Backends/Vulkan/TransferCommandEncoder.h"

namespace Flourish::Vulkan
{
    struct CommandBufferSubmissionData
    {
        std::vector<VkSubmitInfo> GraphicsSubmitInfos;
        std::vector<VkSubmitInfo> ComputeSubmitInfos;
        std::vector<VkSubmitInfo> TransferSubmitInfos;
        std::vector<VkSubmitInfo> SubmitInfos;
        std::vector<VkTimelineSemaphoreSubmitInfo> TimelineSubmitInfos;
        std::vector<u64> SyncSemaphoreValues;
        std::array<VkSemaphore, Flourish::Context::MaxFrameBufferCount> SyncSemaphores;
        VkPipelineStageFlags DrawWaitStages[1] = { VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT };
        VkPipelineStageFlags TransferWaitStages[1] = { VK_PIPELINE_STAGE_TRANSFER_BIT };
        VkPipelineStageFlags ComputeWaitStages[1] = { VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT };
        VkSubmitInfo* FirstSubmitInfo;
        VkSubmitInfo* LastSubmitInfo;
        VkPipelineStageFlags FirstSubBufferWaitStage;
    };

    struct CommandBufferEncoderSubmission
    {
        CommandBufferEncoderSubmission(VkCommandBuffer buffer, const CommandBufferAllocInfo& allocInfo)
            : Buffer(buffer), AllocInfo(allocInfo)
        {}

        VkCommandBuffer Buffer;
        CommandBufferAllocInfo AllocInfo;
    };

    class CommandBuffer : public Flourish::CommandBuffer
    {
    public:
        CommandBuffer(const CommandBufferCreateInfo& createInfo, bool secondary = false);
        ~CommandBuffer() override;

        void SubmitEncodedCommands(VkCommandBuffer buffer, const CommandBufferAllocInfo& allocInfo);
        void ClearSubmissions();
        Flourish::GraphicsCommandEncoder* EncodeGraphicsCommands() override;
        Flourish::RenderCommandEncoder* EncodeRenderCommands(Flourish::Framebuffer* framebuffer) override;
        Flourish::ComputeCommandEncoder* EncodeComputeCommands(Flourish::ComputeTarget* target) override;
        Flourish::TransferCommandEncoder* EncodeTransferCommands() override;

        inline CommandBufferSubmissionData& GetSubmissionData() { return m_SubmissionData; }
        inline void SetLastSubmitFrame(u64 frame) { m_LastSubmitFrame = frame; }

        // TS
        inline const auto& GetEncoderSubmissions() { CheckFrameUpdate(); return m_EncoderSubmissions; }
        inline u64 GetFinalSemaphoreValue() const { return m_SemaphoreBaseValue + m_EncoderSubmissions.size() + 1; }
        inline u64 GetLastSubmitFrame() const { return m_LastSubmitFrame; }

    private:
        void CheckFrameUpdate();

    private:
        u64 m_LastFrameEncoding = 0;
        u64 m_LastSubmitFrame = 0;
        u64 m_SemaphoreBaseValue = 1;
        GraphicsCommandEncoder m_GraphicsCommandEncoder;
        RenderCommandEncoder m_RenderCommandEncoder;
        ComputeCommandEncoder m_ComputeCommandEncoder;
        TransferCommandEncoder m_TransferCommandEncoder;
        std::vector<CommandBufferEncoderSubmission> m_EncoderSubmissions;
        CommandBufferSubmissionData m_SubmissionData;
    };
}