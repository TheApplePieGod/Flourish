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
        
    private:
        void CheckFrameUpdate();

    private:
        u32 m_LastFrameEncoding = 0;
        std::vector<RenderCommandEncoder> m_RenderCommandEncoderCache;
        u32 m_RenderCommandEncoderCachePtr = 0;
        std::vector<EncoderSubmission> m_EncoderSubmissions;
        std::thread::id m_AllocatedThread;
    };
}