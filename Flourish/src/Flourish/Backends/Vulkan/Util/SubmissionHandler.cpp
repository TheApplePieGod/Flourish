#include "flpch.h"
#include "SubmissionHandler.h"

#include "Flourish/Backends/Vulkan/Util/Context.h"
#include "Flourish/Backends/Vulkan/RenderContext.h"
#include "Flourish/Backends/Vulkan/CommandBuffer.h"

namespace Flourish::Vulkan
{
    void SubmissionHandler::Initialize()
    {
        
    }
    
    void SubmissionHandler::Shutdown()
    {
        auto semaphorePools = m_SemaphorePools;
        Context::DeleteQueue().Push([=]()
        {
            for (u32 frame = 0; frame < Flourish::Context::FrameBufferCount(); frame++)
                for (auto semaphore : semaphorePools[frame])
                    vkDestroySemaphore(Context::Devices().Device(), semaphore, nullptr);
        });
    }

    void SubmissionHandler::ProcessSubmissions()
    {
        m_SemaphorePoolIndex = 0;
        
        u32 submissionStartIndex = 0;
        u64 primarySyncSemaphoreValue = 0;
        VkPipelineStageFlags drawWaitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        VkPipelineStageFlags transferWaitStages[] = { VK_PIPELINE_STAGE_TRANSFER_BIT };
        VkPipelineStageFlags computeWaitStages[] = { VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT };
        std::vector<VkSubmitInfo> graphicsSubmitInfos;
        std::vector<VkSubmitInfo> computeSubmitInfos;
        std::vector<VkSubmitInfo> transferSubmitInfos;
        
        // TODO: move buffers to a permanent member variable.
        std::vector<u64> semaphoreValues;
        semaphoreValues.reserve(150);
        std::vector<VkTimelineSemaphoreSubmitInfo> timelineSubmitInfos;
        timelineSubmitInfos.reserve(150);
        std::vector<VkCommandBuffer> commandBuffers;
        commandBuffers.reserve(150);
        std::vector<VkSemaphore> syncSemaphores;
        syncSemaphores.reserve(150);

        for (auto submissionCount : Flourish::Context::SubmittedCommandBufferCounts())
        {
            VkSemaphore primarySyncSemaphore = GetSemaphore();

            for (u32 submissionIndex = submissionStartIndex; submissionIndex < submissionStartIndex + submissionCount; submissionIndex++)
            {
                auto& submission = Flourish::Context::SubmittedCommandBuffers()[submissionIndex];
                for (auto _buffer : submission)
                {
                    u64 syncSemaphoreValue = 0;
                    VkSemaphore syncSemaphore = GetSemaphore();
                    auto buffer = static_cast<const CommandBuffer*>(_buffer);
                    for (u32 i = 0; i < buffer->GetEncoderSubmissions().size(); i++)
                    {
                        const auto& encodedSubmission = buffer->GetEncoderSubmissions()[i];
                        bool isLastSubmission = i == buffer->GetEncoderSubmissions().size() - 1;

                        u32 semaphoreValuesStartIndex = semaphoreValues.size();
                        u32 syncSemaphoresStartIndex = syncSemaphores.size();
                        VkSubmitInfo encodedCommandSubmitInfo{};
                        encodedCommandSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
                        encodedCommandSubmitInfo.pNext = timelineSubmitInfos.data() + timelineSubmitInfos.size();
                        encodedCommandSubmitInfo.pWaitDstStageMask = drawWaitStages;
                        encodedCommandSubmitInfo.commandBufferCount = 1;
                        encodedCommandSubmitInfo.pCommandBuffers = commandBuffers.data() + commandBuffers.size();

                        if (primarySyncSemaphoreValue > 0)
                        {
                            encodedCommandSubmitInfo.waitSemaphoreCount++;
                            syncSemaphores.push_back(primarySyncSemaphore);
                            semaphoreValues.push_back(primarySyncSemaphoreValue);
                        }
                        if (syncSemaphoreValue > 0)
                        {
                            encodedCommandSubmitInfo.waitSemaphoreCount++;
                            syncSemaphores.push_back(syncSemaphore);
                            semaphoreValues.push_back(syncSemaphoreValue++);
                        }
                        encodedCommandSubmitInfo.pWaitSemaphores = syncSemaphores.data() + syncSemaphoresStartIndex;

                        encodedCommandSubmitInfo.signalSemaphoreCount = 1;
                        syncSemaphores.push_back(syncSemaphore);
                        semaphoreValues.push_back(syncSemaphoreValue);
                        if (isLastSubmission)
                        {
                            encodedCommandSubmitInfo.signalSemaphoreCount++;
                            syncSemaphores.push_back(primarySyncSemaphore);
                            semaphoreValues.push_back(++primarySyncSemaphoreValue);
                        }
                        encodedCommandSubmitInfo.pSignalSemaphores = syncSemaphores.data() + syncSemaphoresStartIndex + encodedCommandSubmitInfo.waitSemaphoreCount;
                        
                        VkTimelineSemaphoreSubmitInfo timelineSubmitInfo{};
                        timelineSubmitInfo.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
                        timelineSubmitInfo.waitSemaphoreValueCount = 1;
                        timelineSubmitInfo.pWaitSemaphoreValues = semaphoreSignalValues.data() + semaphoreSignalValues.size();
                        timelineSubmitInfo.signalSemaphoreValueCount = 1;
                        timelineSubmitInfo.pSignalSemaphoreValues = semaphoreSignalValues.data() + semaphoreSignalValues.size() + 1;

                        semaphoreSignalValues.push_back(syncSemaphoreValue++);
                        semaphoreSignalValues.push_back(syncSemaphoreValue);
                    }
                }
            }

            submissionStartIndex += submissionCount;
        }
        
        for (auto renderContext : m_PresentingContexts)
        {

        }
        
        m_PresentingContexts.clear();
    }

    void SubmissionHandler::PresentRenderContext(const RenderContext* context)
    {
        m_PresentingContextsLock.lock();
        m_PresentingContexts.push_back(context);
        m_PresentingContextsLock.unlock();
    }
    
    VkSemaphore SubmissionHandler::GetSemaphore()
    {
        auto& pool = m_SemaphorePools[Flourish::Context::FrameIndex()];
        if (m_SemaphorePoolIndex >= pool.size())
        {
            VkSemaphoreTypeCreateInfo timelineType{};
            timelineType.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
            timelineType.pNext = NULL;
            timelineType.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
            timelineType.initialValue = 0;

            VkSemaphoreCreateInfo semaphoreInfo{};
            semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
            semaphoreInfo.pNext = &timelineType;

            VkSemaphore semaphore;
            FL_VK_ENSURE_RESULT(vkCreateSemaphore(
                Context::Devices().Device(),
                &semaphoreInfo,
                nullptr,
                &semaphore
            ));
            pool.push_back(semaphore);
        }
        
        return pool[m_SemaphorePoolIndex];
    }
}