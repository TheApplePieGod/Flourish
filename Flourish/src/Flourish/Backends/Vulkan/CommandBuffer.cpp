#include "flpch.h"
#include "CommandBuffer.h"

#include "Flourish/Backends/Vulkan/Context.h"
#include "Flourish/Api/RenderCommandEncoder.h"
#include "Flourish/Backends/Vulkan/Framebuffer.h"
#include "Flourish/Backends/Vulkan/Util/Synchronization.h"

namespace Flourish::Vulkan
{
    CommandBuffer::CommandBuffer(const CommandBufferCreateInfo& createInfo)
        : Flourish::CommandBuffer(createInfo)
    {
        m_GraphicsCommandEncoder = GraphicsCommandEncoder(this, m_Info.FrameRestricted);
        m_RenderCommandEncoder = RenderCommandEncoder(this, m_Info.FrameRestricted);
        m_ComputeCommandEncoder = ComputeCommandEncoder(this, m_Info.FrameRestricted);
        m_TransferCommandEncoder = TransferCommandEncoder(this, m_Info.FrameRestricted);

        if (m_Info.MaxTimestamps > 0)
        {
            m_QueryPoolCount = m_Info.FrameRestricted ? Flourish::Context::FrameBufferCount() : 1;

            // Submit initial command to reset all query pools. This ensures any subsequent read
            // of timestamps is always valid, even if nothing has been written yet
            Context::Commands().SubmitSingleTimeCommands(
                GPUWorkloadType::Graphics, false,
                [this](VkCommandBuffer cmdBuf)
                {
                    VkQueryPoolCreateInfo createInfo{};
                    createInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
                    createInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
                    createInfo.queryCount = m_Info.MaxTimestamps;

                    for (u32 f = 0; f < m_QueryPoolCount; f++)
                    {
                        vkCreateQueryPool(Context::Devices().Device(), &createInfo, nullptr, &m_QueryPools[f]);
                        vkCmdResetQueryPool(cmdBuf, m_QueryPools[f], 0, m_Info.MaxTimestamps);
                    }
                }
            );
        }
    }

    CommandBuffer::~CommandBuffer()
    {
        if (m_QueryPoolCount == 0) return;

        auto poolCount = m_QueryPoolCount;
        auto pools = m_QueryPools;
        Context::FinalizerQueue().Push([=]()
        {
            for (u32 f = 0; f < poolCount; f++)
                vkDestroyQueryPool(Context::Devices().Device(), pools[f], nullptr);
        }, "CommandBuffer querypool free");
    }
    
    void CommandBuffer::SubmitEncodedCommands(const CommandBufferEncoderSubmission& submission)
    {
        m_EncoderSubmissions.emplace_back(submission);
        m_Encoding = false;
    }

    Flourish::GraphicsCommandEncoder* CommandBuffer::EncodeGraphicsCommands()
    {
        CheckFrameUpdate();

        FL_CRASH_ASSERT(!m_Encoding, "Cannot begin encoding while another encoding is in progress");
        m_Encoding = true;

        m_GraphicsCommandEncoder.BeginEncoding();

        return static_cast<Flourish::GraphicsCommandEncoder*>(&m_GraphicsCommandEncoder);
    }

    Flourish::RenderCommandEncoder* CommandBuffer::EncodeRenderCommands(Flourish::Framebuffer* framebuffer)
    {
        CheckFrameUpdate();

        FL_CRASH_ASSERT(!m_Encoding, "Cannot begin encoding while another encoding is in progress");
        m_Encoding = true;
        
        m_RenderCommandEncoder.BeginEncoding(
            static_cast<Framebuffer*>(framebuffer)
        );

        return static_cast<Flourish::RenderCommandEncoder*>(&m_RenderCommandEncoder);
    }

    Flourish::ComputeCommandEncoder* CommandBuffer::EncodeComputeCommands()
    {
        CheckFrameUpdate();

        FL_CRASH_ASSERT(!m_Encoding, "Cannot begin encoding while another encoding is in progress");
        m_Encoding = true;

        m_ComputeCommandEncoder.BeginEncoding();

        return static_cast<Flourish::ComputeCommandEncoder*>(&m_ComputeCommandEncoder);
    }

    Flourish::TransferCommandEncoder* CommandBuffer::EncodeTransferCommands()
    {
        CheckFrameUpdate();

        FL_CRASH_ASSERT(!m_Encoding, "Cannot begin encoding while another encoding is in progress");
        m_Encoding = true;

        m_TransferCommandEncoder.BeginEncoding();

        return static_cast<Flourish::TransferCommandEncoder*>(&m_TransferCommandEncoder);
    }

    d64 CommandBuffer::GetTimestampValue(u32 timestampId)
    {
        FL_CRASH_ASSERT(m_LastFrameEncoding != Flourish::Context::FrameCount(), "GetTimestampValue must be called before any commands are encoded for the frame");
        FL_CRASH_ASSERT(timestampId < GetMaxTimestamps(), "GetTimestampValue timestampId larger than command buffer max timestamps");

        u64 value = 0;
        vkGetQueryPoolResults(
            Context::Devices().Device(),
            GetQueryPool(),
            timestampId, 1,
            sizeof(u64),
            &value,
            sizeof(u64),
            VK_QUERY_RESULT_64_BIT
        );

        return (double)value * (double)Context::Devices().PhysicalDeviceProperties().limits.timestampPeriod;
    }

    VkQueryPool CommandBuffer::GetQueryPool() const
    {
        if (m_QueryPoolCount == 1) return m_QueryPools[0];
        return m_QueryPools[Flourish::Context::FrameIndex()];
    }

    void CommandBuffer::CheckFrameUpdate()
    {
        if (!m_Info.FrameRestricted) return;
            
        if (m_LastFrameEncoding != Flourish::Context::FrameCount())
        {
            m_LastFrameEncoding = Flourish::Context::FrameCount();
            
            // Each new frame, we need to clear the previous encoder submissions
            ClearSubmissions();
        }
    }

    void CommandBuffer::ResetQueryPool(VkCommandBuffer buffer)
    {
        VkQueryPool pool = GetQueryPool();

        if (!pool) return;

        vkCmdResetQueryPool(buffer, pool, 0, m_Info.MaxTimestamps);
    }
}
