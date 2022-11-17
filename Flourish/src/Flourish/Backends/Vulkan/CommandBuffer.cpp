#include "flpch.h"
#include "CommandBuffer.h"

#include "Flourish/Backends/Vulkan/Util/Context.h"
#include "Flourish/Api/RenderCommandEncoder.h"
#include "Flourish/Backends/Vulkan/Framebuffer.h"
#include "Flourish/Backends/Vulkan/ComputeTarget.h"
#include "Flourish/Backends/Vulkan/Util/Synchronization.h"

namespace Flourish::Vulkan
{
    CommandBuffer::CommandBuffer(const CommandBufferCreateInfo& createInfo, bool isPrimary)
        : Flourish::CommandBuffer(createInfo),
          m_AllocatedThread(std::this_thread::get_id())
    {
        m_EncoderSubmissions.reserve(m_Info.MaxEncoders);

        m_SubmissionData.GraphicsSubmitInfos.reserve(m_Info.MaxEncoders);
        m_SubmissionData.ComputeSubmitInfos.reserve(m_Info.MaxEncoders);
        m_SubmissionData.TransferSubmitInfos.reserve(m_Info.MaxEncoders);
        m_SubmissionData.TimelineSubmitInfos.reserve(m_Info.MaxEncoders);
        m_SubmissionData.SyncSemaphoreValues.reserve(m_Info.MaxEncoders);
        
        for (u32 frame = 0; frame < Flourish::Context::FrameBufferCount(); frame++)
            m_SubmissionData.SyncSemaphores[frame] = Synchronization::CreateTimelineSemaphore(0);
    }

    CommandBuffer::~CommandBuffer()
    {
        FL_CRASH_ASSERT(
            m_AllocatedThread == std::this_thread::get_id(),
            "Command buffer should never be destroyed from a thread different than the one that created it"
        );
        
        auto semaphores = m_SubmissionData.SyncSemaphores;
        Context::DeleteQueue().Push([=]()
        {
            for (u32 frame = 0; frame < Flourish::Context::FrameBufferCount(); frame++)
                vkDestroySemaphore(Context::Devices().Device(), semaphores[frame], nullptr);
        }, "Command buffer free");
    }
    
    void CommandBuffer::SubmitEncodedCommands(VkCommandBuffer buffer, GPUWorkloadType workloadType)
    {
        m_EncoderSubmissions.emplace_back(buffer, workloadType);
        m_Encoding = false;
        
        u64 semaphoreBaseValue = Flourish::Context::FrameCount();
        VkSemaphore* syncSemaphore = &m_SubmissionData.SyncSemaphores[Flourish::Context::FrameIndex()];
        
        // Retrieve submission from the appropriate queue 
        // Only set the stage for non-first sub buffers because they will be overridden later
        VkSubmitInfo* encodedCommandSubmitInfo;
        switch (workloadType)
        {
            case GPUWorkloadType::Graphics:
            {
                m_SubmissionData.FinalSubBufferWaitStage = *m_SubmissionData.DrawWaitStages;
                encodedCommandSubmitInfo = &m_SubmissionData.GraphicsSubmitInfos.emplace_back();
                if (m_EncoderSubmissions.size() > 1)
                    encodedCommandSubmitInfo->pWaitDstStageMask = m_SubmissionData.DrawWaitStages;
            } break;
            case GPUWorkloadType::Compute:
            {
                m_SubmissionData.FinalSubBufferWaitStage = *m_SubmissionData.ComputeWaitStages;
                encodedCommandSubmitInfo = &m_SubmissionData.ComputeSubmitInfos.emplace_back();
                if (m_EncoderSubmissions.size() > 1)
                    encodedCommandSubmitInfo->pWaitDstStageMask = m_SubmissionData.ComputeWaitStages;
            } break;
            case GPUWorkloadType::Transfer:
            {
                m_SubmissionData.FinalSubBufferWaitStage = *m_SubmissionData.TransferWaitStages;
                encodedCommandSubmitInfo = &m_SubmissionData.TransferSubmitInfos.emplace_back();
                if (m_EncoderSubmissions.size() > 1)
                    encodedCommandSubmitInfo->pWaitDstStageMask = m_SubmissionData.TransferWaitStages;
            } break;
        }

        encodedCommandSubmitInfo->sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        encodedCommandSubmitInfo->pNext = m_SubmissionData.TimelineSubmitInfos.data() + m_SubmissionData.TransferSubmitInfos.size();
        encodedCommandSubmitInfo->commandBufferCount = 1;
        encodedCommandSubmitInfo->pCommandBuffers = &m_EncoderSubmissions.back().Buffer;
        // Always signaling next sub buffer or potentially completion
        encodedCommandSubmitInfo->signalSemaphoreCount = 1;
        encodedCommandSubmitInfo->pSignalSemaphores = syncSemaphore;
        encodedCommandSubmitInfo->waitSemaphoreCount = 0;

        VkTimelineSemaphoreSubmitInfo* timelineSubmitInfo = &m_SubmissionData.TimelineSubmitInfos.emplace_back();
        timelineSubmitInfo->sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
        timelineSubmitInfo->signalSemaphoreValueCount = 1;
        timelineSubmitInfo->pSignalSemaphoreValues = m_SubmissionData.SyncSemaphoreValues.data() + m_SubmissionData.SyncSemaphoreValues.size();
        m_SubmissionData.SyncSemaphoreValues.push_back(semaphoreBaseValue + m_EncoderSubmissions.size() + 1);

        timelineSubmitInfo->waitSemaphoreValueCount = 0;
        timelineSubmitInfo->pWaitSemaphoreValues = m_SubmissionData.SyncSemaphoreValues.data() + m_SubmissionData.SyncSemaphoreValues.size();
        m_SubmissionData.SyncSemaphoreValues.push_back(semaphoreBaseValue + m_EncoderSubmissions.size());

        // Wait for the last sub buffer if this is not the first one
        if (m_EncoderSubmissions.size() > 1)
        {
            encodedCommandSubmitInfo->waitSemaphoreCount = 1;
            encodedCommandSubmitInfo->pWaitSemaphores = syncSemaphore;
            timelineSubmitInfo->waitSemaphoreValueCount = 1;
            timelineSubmitInfo->pWaitSemaphoreValues = m_SubmissionData.SyncSemaphoreValues.data() + m_SubmissionData.SyncSemaphoreValues.size();

            m_SubmissionData.SyncSemaphoreValues.push_back(semaphoreBaseValue + m_EncoderSubmissions.size());
        }
        else 
            m_SubmissionData.FirstSubmitInfo = encodedCommandSubmitInfo;
    }

    Flourish::RenderCommandEncoder* CommandBuffer::EncodeRenderCommands(Flourish::Framebuffer* framebuffer)
    {
        FL_CRASH_ASSERT(!m_Encoding, "Cannot begin encoding while another encoding is in progress");
        FL_CRASH_ASSERT(m_EncoderSubmissions.size() < m_Info.MaxEncoders, "Cannot exceed maximum encoder count");
        m_Encoding = true;

        CheckFrameUpdate();

        if (m_RenderCommandEncoderCachePtr >= m_RenderCommandEncoderCache.size())
            m_RenderCommandEncoderCache.emplace_back(this);

        m_RenderCommandEncoderCache[m_RenderCommandEncoderCachePtr].BeginEncoding(
            static_cast<Framebuffer*>(framebuffer)
        );

        return static_cast<Flourish::RenderCommandEncoder*>(&m_RenderCommandEncoderCache[m_RenderCommandEncoderCachePtr++]);
    }

    Flourish::ComputeCommandEncoder* CommandBuffer::EncodeComputeCommands(Flourish::ComputeTarget* target)
    {
        FL_CRASH_ASSERT(!m_Encoding, "Cannot begin encoding while another encoding is in progress");
        FL_CRASH_ASSERT(m_EncoderSubmissions.size() < m_Info.MaxEncoders, "Cannot exceed maximum encoder count");
        m_Encoding = true;

        CheckFrameUpdate();

        if (m_ComputeCommandEncoderCachePtr >= m_ComputeCommandEncoderCache.size())
            m_ComputeCommandEncoderCache.emplace_back(this);

        m_ComputeCommandEncoderCache[m_ComputeCommandEncoderCachePtr].BeginEncoding(
            static_cast<ComputeTarget*>(target)
        );

        return static_cast<Flourish::ComputeCommandEncoder*>(&m_ComputeCommandEncoderCache[m_ComputeCommandEncoderCachePtr++]);
    }
    
    void CommandBuffer::CheckFrameUpdate()
    {
        // Each new frame, we need to clear the previous encoder submissions
        if (m_LastFrameEncoding != Flourish::Context::FrameCount())
        {
            m_EncoderSubmissions.clear();
            m_LastFrameEncoding = Flourish::Context::FrameCount();
            m_RenderCommandEncoderCachePtr = 0;
            m_ComputeCommandEncoderCachePtr = 0;
            
            m_SubmissionData.GraphicsSubmitInfos.clear();
            m_SubmissionData.ComputeSubmitInfos.clear();
            m_SubmissionData.TransferSubmitInfos.clear();
            m_SubmissionData.TimelineSubmitInfos.clear();
            m_SubmissionData.SyncSemaphoreValues.clear();
        }
    }
}