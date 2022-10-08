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
        u32 completionSemaphoresStartIndex = 0;
        u32 completionSemaphoresWaitCount = 0;
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
        std::vector<VkSemaphore> completionSemaphores;
        completionSemaphores.reserve(150);
        std::vector<u64> completionSemaphoreValues;
        completionSemaphoreValues.reserve(150);
        std::vector<ProcessedSubmissionInfo> processedSubmissionInfo;

        // Each submission gets executed in parallel
        for (auto submissionCount : Flourish::Context::SubmittedCommandBufferCounts())
        {
            // Each submission executes buffers sequentially
            for (u32 submissionIndex = submissionStartIndex; submissionIndex < submissionStartIndex + submissionCount; submissionIndex++)
            {
                // Each buffer in this submission executes in parallel
                auto& submission = Flourish::Context::SubmittedCommandBuffers()[submissionIndex];
                for (auto _buffer : submission)
                {
                    u64 syncSemaphoreValue = 0;
                    
                    // Create a semaphore that will be used to sync sub buffers and also signal total completion of a buffer
                    VkSemaphore syncSemaphore = GetSemaphore();

                    // Each sub buffer executes sequentially
                    auto buffer = static_cast<const CommandBuffer*>(_buffer);
                    for (u32 i = 0; i < buffer->GetEncoderSubmissions().size(); i++)
                    {
                        const auto& encodedSubmission = buffer->GetEncoderSubmissions()[i];
                        bool isFirstSubBuffer = i == 0;
                        bool isLastSubBuffer = i == buffer->GetEncoderSubmissions().size() - 1;

                        VkSubmitInfo encodedCommandSubmitInfo{};
                        encodedCommandSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
                        encodedCommandSubmitInfo.pNext = timelineSubmitInfos.data() + timelineSubmitInfos.size();
                        encodedCommandSubmitInfo.commandBufferCount = 1;
                        encodedCommandSubmitInfo.pCommandBuffers = &encodedSubmission.Buffer;

                        VkTimelineSemaphoreSubmitInfo timelineSubmitInfo{};
                        timelineSubmitInfo.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
                        
                        // Last sub buffer of the batch must signal the batch completion semaphore
                        if (isLastSubBuffer)
                        {
                            encodedCommandSubmitInfo.signalSemaphoreCount = 1;
                            encodedCommandSubmitInfo.pSignalSemaphores = completionSemaphores.data() + completionSemaphores.size();
                            timelineSubmitInfo.signalSemaphoreValueCount = 1;
                            timelineSubmitInfo.pSignalSemaphoreValues = completionSemaphoreValues.data() + completionSemaphoreValues.size();

                            completionSemaphores.push_back(syncSemaphore);
                            completionSemaphoreValues.push_back(syncSemaphoreValue + 1);
                        }
                        else
                        {
                            encodedCommandSubmitInfo.signalSemaphoreCount = 1;
                            encodedCommandSubmitInfo.pSignalSemaphores = syncSemaphores.data() + syncSemaphores.size();
                            timelineSubmitInfo.signalSemaphoreValueCount = 1;
                            timelineSubmitInfo.pSignalSemaphoreValues = semaphoreValues.data() + semaphoreValues.size();

                            syncSemaphores.push_back(syncSemaphore);
                            semaphoreValues.push_back(syncSemaphoreValue + 1);
                        }

                        // First sub buffer is not subject to the normal waiting process
                        if (isFirstSubBuffer)
                        {
                            // If this is not the first batch then we must wait on the previous batch to complete
                            if (completionSemaphoresWaitCount > 0)
                            {
                                encodedCommandSubmitInfo.waitSemaphoreCount = completionSemaphoresWaitCount;
                                encodedCommandSubmitInfo.pWaitSemaphores = completionSemaphores.data() + completionSemaphoresStartIndex;
                                timelineSubmitInfo.waitSemaphoreValueCount = completionSemaphoresWaitCount;
                                timelineSubmitInfo.pWaitSemaphoreValues = completionSemaphoreValues.data() + completionSemaphoresStartIndex;
                            }
                        }
                        else // Otherwise wait for the last sub buffer to complete
                        {
                            encodedCommandSubmitInfo.waitSemaphoreCount = 1;
                            encodedCommandSubmitInfo.pWaitSemaphores = syncSemaphores.data() + syncSemaphores.size();
                            timelineSubmitInfo.waitSemaphoreValueCount = 1;
                            timelineSubmitInfo.pWaitSemaphoreValues = semaphoreValues.data() + semaphoreValues.size();

                            syncSemaphores.push_back(syncSemaphore);
                            semaphoreValues.push_back(syncSemaphoreValue);
                        }

                        syncSemaphoreValue++;
                        timelineSubmitInfos.emplace_back(timelineSubmitInfo);

                        // Send submission to the appropriate queue and set the stage
                        switch (encodedSubmission.WorkloadType)
                        {
                            case GPUWorkloadType::Graphics:
                            {
                                encodedCommandSubmitInfo.pWaitDstStageMask = drawWaitStages;
                                graphicsSubmitInfos.emplace_back(encodedCommandSubmitInfo);
                            } break;
                            case GPUWorkloadType::Compute:
                            {
                                encodedCommandSubmitInfo.pWaitDstStageMask = computeWaitStages;
                                computeSubmitInfos.emplace_back(encodedCommandSubmitInfo);
                            } break;
                            case GPUWorkloadType::Transfer:
                            {
                                encodedCommandSubmitInfo.pWaitDstStageMask = transferWaitStages;
                                transferSubmitInfos.emplace_back(encodedCommandSubmitInfo);
                            } break;
                        }
                    }
                }
                
                // If these were the last buffers in this submission, we need to add its information to the
                // processed submission info array
                bool isLastBuffers = submissionIndex == submissionStartIndex + submissionCount - 1;
                if (isLastBuffers)
                {

                }
            }

            submissionStartIndex += submissionCount;
            completionSemaphoresStartIndex += completionSemaphoresWaitCount;
            completionSemaphoresWaitCount = completionSemaphores.size() - completionSemaphoresStartIndex;
        }
        
        for (auto renderContext : m_PresentingContexts)
        {
            u64 waitValues[2] = { App::Get().GetFrameCount() + 1, App::Get().GetFrameCount() + 1 };
            VkTimelineSemaphoreSubmitInfo finalTimelineSubmitInfo{};
            finalTimelineSubmitInfo.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
            finalTimelineSubmitInfo.waitSemaphoreValueCount = 2;
            finalTimelineSubmitInfo.pWaitSemaphoreValues = waitValues;

            VkSemaphore finalWaitSemaphores[] = { m_ImageAvailableSemaphores[m_InFlightFrameIndex], auxDrawSemaphores.empty() ? nullptr : auxDrawSemaphores.back() };
            VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
            VkSubmitInfo finalSubmitInfo{};
            finalSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            finalSubmitInfo.pNext = &finalTimelineSubmitInfo;
            finalSubmitInfo.waitSemaphoreCount = auxDrawSemaphores.empty() ? 1 : 2;
            finalSubmitInfo.pWaitSemaphores = finalWaitSemaphores;
            finalSubmitInfo.pWaitDstStageMask = waitStages;
            finalSubmitInfo.commandBufferCount = 1;
            finalSubmitInfo.pCommandBuffers = &renderContext->CommandBuffer().

            VkSemaphore finalSignalSemaphores[] = { m_RenderFinishedSemaphores[m_InFlightFrameIndex] };
            finalSubmitInfo.signalSemaphoreCount = 1;
            finalSubmitInfo.pSignalSemaphores = finalSignalSemaphores;
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