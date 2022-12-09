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
        m_SubmissionData.SyncSemaphoreValues.reserve(m_Info.MaxEncoders * 2);
        
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
        
        VkSemaphore* syncSemaphore = &m_SubmissionData.SyncSemaphores[Flourish::Context::FrameIndex()];
        
        // Retrieve submission from the appropriate queue 
        // Only set the stage for non-first sub buffers because they will be overridden later
        // On macos, we submit each submission sequentially without grouping, so we need to store them sequentially.
        // otherwise we group them by queue type
        VkSubmitInfo* encodedCommandSubmitInfo;
        switch (workloadType)
        {
            case GPUWorkloadType::Graphics:
            {
                m_SubmissionData.FinalSubBufferWaitStage = *m_SubmissionData.DrawWaitStages;
                #ifdef FL_PLATFORM_MACOS
                    encodedCommandSubmitInfo = &m_SubmissionData.SubmitInfos.emplace_back();
                #else
                    encodedCommandSubmitInfo = &m_SubmissionData.GraphicsSubmitInfos.emplace_back();
                #endif
                if (m_EncoderSubmissions.size() > 1)
                    encodedCommandSubmitInfo->pWaitDstStageMask = m_SubmissionData.DrawWaitStages;
            } break;
            case GPUWorkloadType::Compute:
            {
                m_SubmissionData.FinalSubBufferWaitStage = *m_SubmissionData.ComputeWaitStages;
                #ifdef FL_PLATFORM_MACOS
                    encodedCommandSubmitInfo = &m_SubmissionData.SubmitInfos.emplace_back();
                #else
                    encodedCommandSubmitInfo = &m_SubmissionData.ComputeSubmitInfos.emplace_back();
                #endif
                if (m_EncoderSubmissions.size() > 1)
                    encodedCommandSubmitInfo->pWaitDstStageMask = m_SubmissionData.ComputeWaitStages;
            } break;
            case GPUWorkloadType::Transfer:
            {
                m_SubmissionData.FinalSubBufferWaitStage = *m_SubmissionData.TransferWaitStages;
                #ifdef FL_PLATFORM_MACOS
                    encodedCommandSubmitInfo = &m_SubmissionData.SubmitInfos.emplace_back();
                #else
                    encodedCommandSubmitInfo = &m_SubmissionData.TransferSubmitInfos.emplace_back();
                #endif
                if (m_EncoderSubmissions.size() > 1)
                    encodedCommandSubmitInfo->pWaitDstStageMask = m_SubmissionData.TransferWaitStages;
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

    Flourish::GraphicsCommandEncoder* CommandBuffer::EncodeGraphicsCommands()
    {
        CheckFrameUpdate();

        FL_CRASH_ASSERT(!m_Encoding, "Cannot begin encoding while another encoding is in progress");
        FL_CRASH_ASSERT(m_EncoderSubmissions.size() < m_Info.MaxEncoders, "Cannot exceed maximum encoder count");
        m_Encoding = true;

        if (m_GraphicsCommandEncoderCachePtr >= m_GraphicsCommandEncoderCache.size())
            m_GraphicsCommandEncoderCache.emplace_back(this);

        m_GraphicsCommandEncoderCache[m_GraphicsCommandEncoderCachePtr].BeginEncoding();

        return static_cast<Flourish::GraphicsCommandEncoder*>(&m_GraphicsCommandEncoderCache[m_GraphicsCommandEncoderCachePtr++]);
    }

    Flourish::RenderCommandEncoder* CommandBuffer::EncodeRenderCommands(Flourish::Framebuffer* framebuffer)
    {
        CheckFrameUpdate();

        FL_CRASH_ASSERT(!m_Encoding, "Cannot begin encoding while another encoding is in progress");
        FL_CRASH_ASSERT(m_EncoderSubmissions.size() < m_Info.MaxEncoders, "Cannot exceed maximum encoder count");
        m_Encoding = true;

        if (m_RenderCommandEncoderCachePtr >= m_RenderCommandEncoderCache.size())
            m_RenderCommandEncoderCache.emplace_back(this);

        m_RenderCommandEncoderCache[m_RenderCommandEncoderCachePtr].BeginEncoding(
            static_cast<Framebuffer*>(framebuffer)
        );

        return static_cast<Flourish::RenderCommandEncoder*>(&m_RenderCommandEncoderCache[m_RenderCommandEncoderCachePtr++]);
    }

    Flourish::ComputeCommandEncoder* CommandBuffer::EncodeComputeCommands(Flourish::ComputeTarget* target)
    {
        CheckFrameUpdate();

        FL_CRASH_ASSERT(!m_Encoding, "Cannot begin encoding while another encoding is in progress");
        FL_CRASH_ASSERT(m_EncoderSubmissions.size() < m_Info.MaxEncoders, "Cannot exceed maximum encoder count");
        m_Encoding = true;

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
            m_SemaphoreBaseValue += m_EncoderSubmissions.size() + 1;
            m_EncoderSubmissions.clear();
            m_LastFrameEncoding = Flourish::Context::FrameCount();
            m_RenderCommandEncoderCachePtr = 0;
            m_ComputeCommandEncoderCachePtr = 0;
            
            m_SubmissionData.SubmitInfos.clear();
            m_SubmissionData.GraphicsSubmitInfos.clear();
            m_SubmissionData.ComputeSubmitInfos.clear();
            m_SubmissionData.TransferSubmitInfos.clear();
            m_SubmissionData.TimelineSubmitInfos.clear();
            m_SubmissionData.SyncSemaphoreValues.clear();
        }
    }
}