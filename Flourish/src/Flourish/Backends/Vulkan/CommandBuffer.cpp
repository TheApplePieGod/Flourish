#include "flpch.h"
#include "CommandBuffer.h"

#include "Flourish/Backends/Vulkan/Context.h"
#include "Flourish/Api/RenderCommandEncoder.h"
#include "Flourish/Backends/Vulkan/Framebuffer.h"
#include "Flourish/Backends/Vulkan/ComputeTarget.h"
#include "Flourish/Backends/Vulkan/Util/Synchronization.h"

namespace Flourish::Vulkan
{
    CommandBuffer::CommandBuffer(const CommandBufferCreateInfo& createInfo, bool isPrimary)
        : Flourish::CommandBuffer(createInfo)
    {
        if (m_Info.MaxEncoders == 0)
        {
            FL_LOG_ERROR("Cannot create a command buffer with no encoders");
            throw std::exception();
        }

        m_EncoderSubmissions.reserve(m_Info.MaxEncoders);

        m_SubmissionData.SubmitInfos.reserve(m_Info.MaxEncoders);
        m_SubmissionData.TimelineSubmitInfos.reserve(m_Info.MaxEncoders);
        m_SubmissionData.SyncSemaphoreValues.reserve(m_Info.MaxEncoders * 2);
        
        m_GraphicsCommandEncoder = GraphicsCommandEncoder(this, m_Info.FrameRestricted);
        m_RenderCommandEncoder = RenderCommandEncoder(this, m_Info.FrameRestricted);
        m_ComputeCommandEncoder = ComputeCommandEncoder(this, m_Info.FrameRestricted);
        m_TransferCommandEncoder = TransferCommandEncoder(this, m_Info.FrameRestricted);
        
        for (u32 frame = 0; frame < Flourish::Context::FrameBufferCount(); frame++)
            m_SubmissionData.SyncSemaphores[frame] = Synchronization::CreateTimelineSemaphore(0);
    }

    CommandBuffer::~CommandBuffer()
    {
        auto semaphores = m_SubmissionData.SyncSemaphores;
        Context::FinalizerQueue().Push([=]()
        {
            for (u32 frame = 0; frame < Flourish::Context::FrameBufferCount(); frame++)
                if (semaphores[frame])
                    vkDestroySemaphore(Context::Devices().Device(), semaphores[frame], nullptr);
        }, "Command buffer free");
    }
    
    void CommandBuffer::SubmitEncodedCommands(VkCommandBuffer buffer, const CommandBufferAllocInfo& allocInfo)
    {
        m_EncoderSubmissions.emplace_back(buffer, allocInfo);
        m_Encoding = false;
        
        VkSemaphore* syncSemaphore = &m_SubmissionData.SyncSemaphores[Flourish::Context::FrameIndex()];
        
        // Retrieve submission from the appropriate queue 
        // Only set the stage for non-first sub buffers because they will be overridden later
        // On macos, we submit each submission sequentially without grouping, so we need to store them sequentially.
        // otherwise we group them by queue type
        VkSubmitInfo* encodedCommandSubmitInfo;
        switch (allocInfo.WorkloadType)
        {
            case GPUWorkloadType::Graphics:
            {
                encodedCommandSubmitInfo = &m_SubmissionData.SubmitInfos.emplace_back();
                if (m_EncoderSubmissions.size() > 1)
                    encodedCommandSubmitInfo->pWaitDstStageMask = m_SubmissionData.DrawWaitStages;
                else
                    m_SubmissionData.FirstSubBufferWaitStage = *m_SubmissionData.DrawWaitStages;
            } break;
            case GPUWorkloadType::Compute:
            {
                encodedCommandSubmitInfo = &m_SubmissionData.SubmitInfos.emplace_back();
                if (m_EncoderSubmissions.size() > 1)
                    encodedCommandSubmitInfo->pWaitDstStageMask = m_SubmissionData.ComputeWaitStages;
                else
                    m_SubmissionData.FirstSubBufferWaitStage = *m_SubmissionData.ComputeWaitStages;
            } break;
            case GPUWorkloadType::Transfer:
            {
                encodedCommandSubmitInfo = &m_SubmissionData.SubmitInfos.emplace_back();
                if (m_EncoderSubmissions.size() > 1)
                    encodedCommandSubmitInfo->pWaitDstStageMask = m_SubmissionData.TransferWaitStages;
                else
                    m_SubmissionData.FirstSubBufferWaitStage = *m_SubmissionData.TransferWaitStages;
            } break;
        }

        encodedCommandSubmitInfo->sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        encodedCommandSubmitInfo->pNext = m_SubmissionData.TimelineSubmitInfos.data() + m_SubmissionData.TimelineSubmitInfos.size();
        encodedCommandSubmitInfo->commandBufferCount = 1;
        encodedCommandSubmitInfo->pCommandBuffers = &m_EncoderSubmissions.back().Buffer;
        // Always signaling next sub buffer or potentially completion
        encodedCommandSubmitInfo->signalSemaphoreCount = 1;
        encodedCommandSubmitInfo->pSignalSemaphores = syncSemaphore;
        encodedCommandSubmitInfo->waitSemaphoreCount = 0;

        VkTimelineSemaphoreSubmitInfo* timelineSubmitInfo = &m_SubmissionData.TimelineSubmitInfos.emplace_back();
        timelineSubmitInfo->sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
        timelineSubmitInfo->waitSemaphoreValueCount = 0;
        timelineSubmitInfo->signalSemaphoreValueCount = 1;
        timelineSubmitInfo->pSignalSemaphoreValues = m_SubmissionData.SyncSemaphoreValues.data() + m_SubmissionData.SyncSemaphoreValues.size();
        m_SubmissionData.SyncSemaphoreValues.push_back(m_SemaphoreBaseValue + m_EncoderSubmissions.size() + 1);

        // Wait for the last sub buffer if this is not the first one
        if (m_EncoderSubmissions.size() > 1)
        {
            encodedCommandSubmitInfo->waitSemaphoreCount = 1;
            encodedCommandSubmitInfo->pWaitSemaphores = syncSemaphore;
            timelineSubmitInfo->waitSemaphoreValueCount = 1;
            timelineSubmitInfo->pWaitSemaphoreValues = m_SubmissionData.SyncSemaphoreValues.data() + m_SubmissionData.SyncSemaphoreValues.size();

            m_SubmissionData.SyncSemaphoreValues.push_back(m_SemaphoreBaseValue + m_EncoderSubmissions.size());
        }
        else 
            m_SubmissionData.FirstSubmitInfo = encodedCommandSubmitInfo;
        
        m_SubmissionData.LastSubmitInfo = encodedCommandSubmitInfo;
    }

    void CommandBuffer::ClearSubmissions()
    {
        m_SemaphoreBaseValue += m_EncoderSubmissions.size() + 1;
        m_EncoderSubmissions.clear();
        
        m_SubmissionData.SubmitInfos.clear();
        m_SubmissionData.TimelineSubmitInfos.clear();
        m_SubmissionData.SyncSemaphoreValues.clear();
    }

    Flourish::GraphicsCommandEncoder* CommandBuffer::EncodeGraphicsCommands()
    {
        CheckFrameUpdate();

        FL_CRASH_ASSERT(!m_Encoding, "Cannot begin encoding while another encoding is in progress");
        FL_CRASH_ASSERT(m_EncoderSubmissions.size() < m_Info.MaxEncoders, "Cannot exceed maximum encoder count");
        m_Encoding = true;

        m_GraphicsCommandEncoder.BeginEncoding();

        return static_cast<Flourish::GraphicsCommandEncoder*>(&m_GraphicsCommandEncoder);
    }

    Flourish::RenderCommandEncoder* CommandBuffer::EncodeRenderCommands(Flourish::Framebuffer* framebuffer)
    {
        CheckFrameUpdate();

        FL_CRASH_ASSERT(!m_Encoding, "Cannot begin encoding while another encoding is in progress");
        FL_CRASH_ASSERT(m_EncoderSubmissions.size() < m_Info.MaxEncoders, "Cannot exceed maximum encoder count");
        m_Encoding = true;
        
        m_RenderCommandEncoder.BeginEncoding(
            static_cast<Framebuffer*>(framebuffer)
        );

        return static_cast<Flourish::RenderCommandEncoder*>(&m_RenderCommandEncoder);
    }

    Flourish::ComputeCommandEncoder* CommandBuffer::EncodeComputeCommands(Flourish::ComputeTarget* target)
    {
        CheckFrameUpdate();

        FL_CRASH_ASSERT(!m_Encoding, "Cannot begin encoding while another encoding is in progress");
        FL_CRASH_ASSERT(m_EncoderSubmissions.size() < m_Info.MaxEncoders, "Cannot exceed maximum encoder count");
        m_Encoding = true;

        m_ComputeCommandEncoder.BeginEncoding(
            static_cast<ComputeTarget*>(target)
        );

        return static_cast<Flourish::ComputeCommandEncoder*>(&m_ComputeCommandEncoder);
    }

    Flourish::TransferCommandEncoder* CommandBuffer::EncodeTransferCommands()
    {
        CheckFrameUpdate();

        FL_CRASH_ASSERT(!m_Encoding, "Cannot begin encoding while another encoding is in progress");
        FL_CRASH_ASSERT(m_EncoderSubmissions.size() < m_Info.MaxEncoders, "Cannot exceed maximum encoder count");
        m_Encoding = true;

        m_TransferCommandEncoder.BeginEncoding();

        return static_cast<Flourish::TransferCommandEncoder*>(&m_TransferCommandEncoder);
    }
    
    void CommandBuffer::CheckFrameUpdate()
    {
        if (!m_Info.FrameRestricted) return;
            
        // Each new frame, we need to clear the previous encoder submissions
        if (m_LastFrameEncoding != Flourish::Context::FrameCount())
        {
            m_LastFrameEncoding = Flourish::Context::FrameCount();
            
            ClearSubmissions();
        }
    }
}