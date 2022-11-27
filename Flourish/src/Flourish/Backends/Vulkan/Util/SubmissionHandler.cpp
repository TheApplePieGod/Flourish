#include "flpch.h"
#include "SubmissionHandler.h"

#include "Flourish/Backends/Vulkan/Util/Context.h"
#include "Flourish/Backends/Vulkan/RenderContext.h"
#include "Flourish/Backends/Vulkan/CommandBuffer.h"

namespace Flourish::Vulkan
{
    void SubmissionHandler::Initialize()
    {
        // This will last for a very high number of submissions
        m_SubmissionData.ExtraSemaphores.reserve(500);
        m_SubmissionData.ExtraSemaphoreValues.reserve(500);
        m_SubmissionData.ExtraWaitStages.reserve(500);
        m_SubmissionData.CompletionSemaphores.reserve(500);
        m_SubmissionData.CompletionSemaphoreValues.reserve(500);
        m_SubmissionData.CompletionWaitStages.reserve(500);
    }
    
    void SubmissionHandler::Shutdown()
    {

    }

    void SubmissionHandler::WaitOnFrameSemaphores()
    {
        if (m_FrameWaitSemaphores[Flourish::Context::FrameIndex()].empty()) return;

        VkSemaphoreWaitInfo waitInfo{};
        waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
        waitInfo.semaphoreCount = m_FrameWaitSemaphores[Flourish::Context::FrameIndex()].size();
        waitInfo.pSemaphores = m_FrameWaitSemaphores[Flourish::Context::FrameIndex()].data();
        waitInfo.pValues = m_FrameWaitSemaphoreValues[Flourish::Context::FrameIndex()].data();

        vkWaitSemaphoresKHR(Context::Devices().Device(), &waitInfo, UINT64_MAX);
    }

    void SubmissionHandler::ProcessSubmissions()
    {
        u32 submissionStartIndex = 0;
        u32 completionSemaphoresAdded = 0;
        u32 completionSemaphoresStartIndex = 0;
        u32 completionSemaphoresWaitCount = 0;
        
        m_SubmissionData.GraphicsSubmitInfos.clear();
        m_SubmissionData.ComputeSubmitInfos.clear();
        m_SubmissionData.TransferSubmitInfos.clear();
        m_SubmissionData.ExtraSemaphores.clear();
        m_SubmissionData.ExtraSemaphoreValues.clear();
        m_SubmissionData.ExtraWaitStages.clear();
        m_SubmissionData.CompletionSemaphores.clear();
        m_SubmissionData.CompletionSemaphoreValues.clear();
        m_SubmissionData.CompletionWaitStages.clear();
        m_FrameWaitSemaphores[Flourish::Context::FrameIndex()].clear();
        m_FrameWaitSemaphoreValues[Flourish::Context::FrameIndex()].clear();

        // Each submission gets executed in parallel
        for (auto submissionCount : Flourish::Context::SubmittedCommandBufferCounts())
        {
            completionSemaphoresWaitCount = 0;
            completionSemaphoresAdded = 0;

            // Each submission executes sub submissions sequentially
            for (u32 submissionIndex = submissionStartIndex; submissionIndex < submissionStartIndex + submissionCount; submissionIndex++)
            {
                bool isLastSubmission = submissionIndex == submissionStartIndex + submissionCount - 1;

                // Each buffer in this sub submission executes in parallel
                auto& submission = Flourish::Context::SubmittedCommandBuffers()[submissionIndex];
                for (auto _buffer : submission)
                {
                    CommandBuffer* buffer = static_cast<CommandBuffer*>(_buffer);
                    if (buffer->GetEncoderSubmissions().empty()) continue; // TODO: warn here?

                    auto& subData = buffer->GetSubmissionData();
                    
                    // If this is not the first batch then we must wait on the previous batch to complete
                    if (completionSemaphoresWaitCount > 0)
                    {
                        if (subData.FirstSubmitInfo->waitSemaphoreCount > 0) // Already semaphores to wait on so we need to include those as well
                        {
                            u32 startingSize = m_SubmissionData.ExtraSemaphores.size();
                            
                            // Copy semaphores
                            m_SubmissionData.ExtraSemaphores.insert(
                                m_SubmissionData.ExtraSemaphores.end(),
                                m_SubmissionData.CompletionSemaphores.begin() + completionSemaphoresStartIndex,
                                m_SubmissionData.CompletionSemaphores.begin() + completionSemaphoresStartIndex + completionSemaphoresWaitCount
                            );
                            for (u32 i = 0; i < subData.FirstSubmitInfo->waitSemaphoreCount; i++)
                                m_SubmissionData.ExtraSemaphores.emplace_back(subData.FirstSubmitInfo->pWaitSemaphores[i]);

                            // Copy values
                            m_SubmissionData.ExtraSemaphoreValues.insert(
                                m_SubmissionData.ExtraSemaphoreValues.end(),
                                m_SubmissionData.CompletionSemaphoreValues.begin() + completionSemaphoresStartIndex,
                                m_SubmissionData.CompletionSemaphoreValues.begin() + completionSemaphoresStartIndex + completionSemaphoresWaitCount
                            );
                            for (u32 i = 0; i < subData.FirstSubmitInfo->waitSemaphoreCount; i++)
                                m_SubmissionData.ExtraSemaphoreValues.emplace_back(subData.TimelineSubmitInfos.front().pWaitSemaphoreValues[i]);

                            // Copy wait stages
                            m_SubmissionData.ExtraWaitStages.insert(
                                m_SubmissionData.ExtraWaitStages.end(),
                                m_SubmissionData.CompletionWaitStages.begin() + completionSemaphoresStartIndex,
                                m_SubmissionData.CompletionWaitStages.begin() + completionSemaphoresStartIndex + completionSemaphoresWaitCount
                            );
                            for (u32 i = 0; i < subData.FirstSubmitInfo->waitSemaphoreCount; i++)
                                m_SubmissionData.ExtraWaitStages.emplace_back(subData.FirstSubmitInfo->pWaitDstStageMask[i]);

                            subData.FirstSubmitInfo->pWaitSemaphores = m_SubmissionData.ExtraSemaphores.data() + startingSize;
                            subData.FirstSubmitInfo->pWaitDstStageMask = m_SubmissionData.ExtraWaitStages.data() + startingSize;
                            subData.TimelineSubmitInfos.front().pWaitSemaphoreValues = m_SubmissionData.ExtraSemaphoreValues.data() + startingSize;
                        }
                        else
                        {
                            subData.FirstSubmitInfo->pWaitSemaphores = m_SubmissionData.CompletionSemaphores.data() + completionSemaphoresStartIndex;
                            subData.FirstSubmitInfo->pWaitDstStageMask = m_SubmissionData.CompletionWaitStages.data() + completionSemaphoresStartIndex;
                            subData.TimelineSubmitInfos.front().pWaitSemaphoreValues = m_SubmissionData.CompletionSemaphoreValues.data() + completionSemaphoresStartIndex;
                        }

                        subData.FirstSubmitInfo->waitSemaphoreCount += completionSemaphoresWaitCount;
                        subData.TimelineSubmitInfos.front().waitSemaphoreValueCount += completionSemaphoresWaitCount;
                    }
                    
                    // Add final sub buffer semaphore to completion list for later awaiting
                    m_SubmissionData.CompletionSemaphores.push_back(subData.SyncSemaphores[Flourish::Context::FrameIndex()]);
                    m_SubmissionData.CompletionSemaphoreValues.push_back(buffer->GetFinalSemaphoreValue());
                    m_SubmissionData.CompletionWaitStages.push_back(subData.FinalSubBufferWaitStage);
                    completionSemaphoresAdded++;

                    // For each submission, add the final semaphores of the final sub submission to the frame wait semaphores
                    // so we can keep track of what needs to be waited on to ensure all processing has been completed
                    if (isLastSubmission)
                    {
                        m_FrameWaitSemaphores[Flourish::Context::FrameIndex()].push_back(m_SubmissionData.CompletionSemaphores.back());
                        m_FrameWaitSemaphoreValues[Flourish::Context::FrameIndex()].push_back(m_SubmissionData.CompletionSemaphoreValues.back());
                    }

                    // If we are on mac, we need to submit each submitinfo individually. Otherwise, we can group them up to be submitted all at once.
                    #ifdef FL_PLATFORM_MACOS
                        for (u32 i = 0; i < subData.SubmitInfos.size(); i++)
                        {
                            FL_VK_ENSURE_RESULT(vkQueueSubmit(
                                Context::Queues().Queue(buffer->GetEncoderSubmissions()[i].WorkloadType),
                                1, &subData.SubmitInfos[i], nullptr
                            ));
                        }
                    #else
                        // Copy submission info
                        m_SubmissionData.GraphicsSubmitInfos.insert(m_SubmissionData.GraphicsSubmitInfos.end(), subData.GraphicsSubmitInfos.begin(), subData.GraphicsSubmitInfos.end());
                        m_SubmissionData.ComputeSubmitInfos.insert(m_SubmissionData.ComputeSubmitInfos.end(), subData.ComputeSubmitInfos.begin(), subData.ComputeSubmitInfos.end());
                        m_SubmissionData.TransferSubmitInfos.insert(m_SubmissionData.TransferSubmitInfos.end(), subData.TransferSubmitInfos.begin(), subData.TransferSubmitInfos.end());
                    #endif
                }

                // Move completion pointer so that next batch will wait on semaphores from last batch
                completionSemaphoresStartIndex += completionSemaphoresWaitCount;
                completionSemaphoresWaitCount = completionSemaphoresAdded;
                completionSemaphoresAdded = 0;
            }
            
            submissionStartIndex += submissionCount;
            completionSemaphoresStartIndex += completionSemaphoresWaitCount;
        }

        if (!m_SubmissionData.GraphicsSubmitInfos.empty())
        {
            FL_VK_ENSURE_RESULT(vkQueueSubmit(
                Context::Queues().Queue(GPUWorkloadType::Graphics),
                static_cast<u32>(m_SubmissionData.GraphicsSubmitInfos.size()),
                m_SubmissionData.GraphicsSubmitInfos.data(),
                nullptr
            ));
        }
        if (!m_SubmissionData.ComputeSubmitInfos.empty())
        {
            FL_VK_ENSURE_RESULT(vkQueueSubmit(
                Context::Queues().Queue(GPUWorkloadType::Compute),
                static_cast<u32>(m_SubmissionData.ComputeSubmitInfos.size()),
                m_SubmissionData.ComputeSubmitInfos.data(),
                nullptr
            ));
        }
        if (!m_SubmissionData.TransferSubmitInfos.empty())
        {
            FL_VK_ENSURE_RESULT(vkQueueSubmit(
                Context::Queues().Queue(GPUWorkloadType::Transfer),
                static_cast<u32>(m_SubmissionData.TransferSubmitInfos.size()),
                m_SubmissionData.TransferSubmitInfos.data(),
                nullptr
            ));
        }

        for (auto context : m_PresentingContexts)
        {
            VkSwapchainKHR swapchain[1] = { context->Swapchain().GetSwapchain() };
            u32 imageIndex[1] = { context->Swapchain().GetActiveImageIndex() };

            VkPresentInfoKHR presentInfo{};
            presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
            presentInfo.waitSemaphoreCount = 1;
            presentInfo.pWaitSemaphores = &context->GetSubmissionData().SignalSemaphores[Flourish::Context::FrameIndex()];
            presentInfo.swapchainCount = 1;
            presentInfo.pSwapchains = swapchain;
            presentInfo.pImageIndices = imageIndex;
        
            auto result = vkQueuePresentKHR(Context::Queues().PresentQueue(), &presentInfo);
            if (result == VK_ERROR_OUT_OF_DATE_KHR)
                context->Swapchain().Recreate();
        }
        
        m_PresentingContexts.clear();
    }

    void SubmissionHandler::PresentRenderContext(RenderContext* context)
    {
        m_PresentingContextsLock.lock();
        m_PresentingContexts.push_back(context);
        m_PresentingContextsLock.unlock();
    }
}