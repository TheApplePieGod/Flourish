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
        VkCommandBuffer GetCommandBuffer() const;

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
        std::vector<RenderCommandEncoder> m_RenderCommandEncoderCache;
        u32 m_RenderCommandEncoderCachePtr = 0;
        std::vector<EncoderSubmission> m_EncoderSubmissions;
        std::array<VkCommandBuffer, Flourish::Context::MaxFrameBufferCount> m_CommandBuffers;
        std::thread::id m_AllocatedThread;
    };
}