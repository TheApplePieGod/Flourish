#pragma once

#include "Flourish/Api/CommandBuffer.h"
#include "Flourish/Backends/Vulkan/Util/Common.h"
#include "Flourish/Backends/Vulkan/GraphicsCommandEncoder.h"
#include "Flourish/Backends/Vulkan/RenderCommandEncoder.h"
#include "Flourish/Backends/Vulkan/ComputeCommandEncoder.h"
#include "Flourish/Backends/Vulkan/TransferCommandEncoder.h"

namespace Flourish::Vulkan
{
    class CommandBuffer : public Flourish::CommandBuffer
    {
    public:
        CommandBuffer(const CommandBufferCreateInfo& createInfo);
        ~CommandBuffer() override;

        void SubmitEncodedCommands(const CommandBufferEncoderSubmission& submission);
        [[nodiscard]] Flourish::GraphicsCommandEncoder* EncodeGraphicsCommands() override;
        [[nodiscard]] Flourish::RenderCommandEncoder* EncodeRenderCommands(Flourish::Framebuffer* framebuffer) override;
        [[nodiscard]] Flourish::ComputeCommandEncoder* EncodeComputeCommands() override;
        [[nodiscard]] Flourish::TransferCommandEncoder* EncodeTransferCommands() override;

        d64 GetTimestampValue(u32 timestampId) override;

        inline void SetLastSubmitFrame(u64 frame) { m_LastSubmitFrame = frame; }
        inline void ClearSubmissions() { m_EncoderSubmissions.clear(); }

        // TS
        VkQueryPool GetQueryPool() const;
        void ResetQueryPool(VkCommandBuffer buffer);

        // TS
        inline const auto& GetEncoderSubmissions() { CheckFrameUpdate(); return m_EncoderSubmissions; }
        inline u64 GetLastSubmitFrame() const { return m_LastSubmitFrame; }

    private:
        void CheckFrameUpdate();

    private:
        u64 m_LastFrameEncoding = 0;
        u64 m_LastSubmitFrame = 0;
        GraphicsCommandEncoder m_GraphicsCommandEncoder;
        RenderCommandEncoder m_RenderCommandEncoder;
        ComputeCommandEncoder m_ComputeCommandEncoder;
        TransferCommandEncoder m_TransferCommandEncoder;
        std::vector<CommandBufferEncoderSubmission> m_EncoderSubmissions;

        u32 m_QueryPoolCount = 0;
        std::array<VkQueryPool, Flourish::Context::MaxFrameBufferCount> m_QueryPools{};
    };
}
