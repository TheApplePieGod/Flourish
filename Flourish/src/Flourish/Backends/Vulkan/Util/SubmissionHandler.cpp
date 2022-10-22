#include "flpch.h"
#include "SubmissionHandler.h"

#include "Flourish/Backends/Vulkan/Util/Context.h"
#include "Flourish/Backends/Vulkan/RenderContext.h"
#include "Flourish/Backends/Vulkan/CommandBuffer.h"
#include "Flourish/Backends/Vulkan/Util/Synchronization.h"

namespace Flourish::Vulkan
{
    void SubmissionHandler::Initialize()
    {
        
    }
    
    void SubmissionHandler::Shutdown()
    {
        auto semaphorePools = m_SemaphorePools;
        auto timelineSemaphorePools = m_TimelineSemaphorePools;
        Context::DeleteQueue().Push([=]()
        {
            for (u32 frame = 0; frame < Flourish::Context::FrameBufferCount(); frame++)
            {
                for (auto semaphore : semaphorePools[frame].Semaphores)
                    vkDestroySemaphore(Context::Devices().Device(), semaphore, nullptr);
                for (auto semaphore : timelineSemaphorePools[frame].Semaphores)
                    vkDestroySemaphore(Context::Devices().Device(), semaphore, nullptr);
            }
        });
    }

    void SubmissionHandler::ProcessSubmissions()
    {
        m_SemaphorePools[Flourish::Context::FrameIndex()].FreeIndex = 0;
        m_TimelineSemaphorePools[Flourish::Context::FrameIndex()].FreeIndex = 0;
        
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
        std::vector<ProcessedSubmissionInfo> processedSubmissions;

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
                    VkSemaphore syncSemaphore = GetTimelineSemaphore();

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
            }
            
            submissionStartIndex += submissionCount;
            completionSemaphoresStartIndex += completionSemaphoresWaitCount;
            completionSemaphoresWaitCount = completionSemaphores.size() - completionSemaphoresStartIndex;

            processedSubmissions.emplace_back(completionSemaphoresStartIndex, completionSemaphoresWaitCount);
        }
        
        std::vector<VkSwapchainKHR> presentingSwapchains;
        presentingSwapchains.reserve(m_PresentingContexts.size());
        std::vector<u32> presentingImageIndices;
        presentingImageIndices.reserve(m_PresentingContexts.size());
        std::vector<VkSemaphore> finalWaitSemaphores;
        finalWaitSemaphores.reserve(150);
        std::vector<u64> finalWaitSemaphoreValues;
        finalWaitSemaphoreValues.reserve(150);
        std::vector<VkPipelineStageFlags> finalWaitStages;
        finalWaitStages.reserve(150);
        std::vector<VkSemaphore> finalSignalSemaphores;
        finalSignalSemaphores.reserve(50);

        // TODO: this wont work with multiple submissions
        for (auto contextSubmission : m_PresentingContexts)
        {
            u32 waitSemaphoreCount = 1;
            finalWaitSemaphores.push_back(contextSubmission.Context->GetImageAvailableSemaphore());
            finalWaitSemaphoreValues.push_back(1);
            finalWaitStages.push_back(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

            if (contextSubmission.DependencySubmissionId != -1)
            {
                auto& processedSubmission = processedSubmissions[contextSubmission.DependencySubmissionId];
                waitSemaphoreCount += processedSubmission.CompletionSemaphoresCount;

                finalWaitSemaphores.insert(
                    finalWaitSemaphores.end(),
                    completionSemaphores.begin() + processedSubmission.CompletionSemaphoresStartIndex,
                    completionSemaphores.begin() + processedSubmission.CompletionSemaphoresStartIndex + processedSubmission.CompletionSemaphoresCount
                );

                finalWaitSemaphoreValues.insert(
                    finalWaitSemaphoreValues.end(),
                    completionSemaphoreValues.begin() + processedSubmission.CompletionSemaphoresStartIndex,
                    completionSemaphoreValues.begin() + processedSubmission.CompletionSemaphoresStartIndex + processedSubmission.CompletionSemaphoresCount
                );

                finalWaitStages.insert(
                    finalWaitStages.end(),
                    processedSubmission.CompletionSemaphoresCount + 1,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                );
            }

            finalSignalSemaphores.push_back(GetSemaphore());

            VkTimelineSemaphoreSubmitInfo finalTimelineSubmitInfo{};
            finalTimelineSubmitInfo.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
            finalTimelineSubmitInfo.waitSemaphoreValueCount = waitSemaphoreCount;
            finalTimelineSubmitInfo.pWaitSemaphoreValues = finalWaitSemaphoreValues.data();

            VkSubmitInfo finalSubmitInfo{};
            finalSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            finalSubmitInfo.pNext = &finalTimelineSubmitInfo;
            finalSubmitInfo.waitSemaphoreCount = waitSemaphoreCount;
            finalSubmitInfo.pWaitSemaphores = finalWaitSemaphores.data();
            finalSubmitInfo.pWaitDstStageMask = finalWaitStages.data();
            finalSubmitInfo.signalSemaphoreCount = 1;
            finalSubmitInfo.pSignalSemaphores = finalSignalSemaphores.data();
            finalSubmitInfo.commandBufferCount = 1;
            finalSubmitInfo.pCommandBuffers = &contextSubmission.Context->CommandBuffer().GetEncoderSubmissions()[0].Buffer;
            
            // TODO: stop doing a copy here
            graphicsSubmitInfos.emplace_back(finalSubmitInfo);
            
            presentingSwapchains.push_back(contextSubmission.Context->Swapchain().GetSwapchain());
            presentingImageIndices.push_back(contextSubmission.Context->Swapchain().GetActiveImageIndex());
        }
        
        if (!graphicsSubmitInfos.empty())
        {
            FL_VK_ENSURE_RESULT(vkQueueSubmit(
                Context::Queues().Queue(GPUWorkloadType::Graphics),
                static_cast<u32>(graphicsSubmitInfos.size()),
                graphicsSubmitInfos.data(),
                Context::Queues().QueueFence(GPUWorkloadType::Graphics)
            ));
        }
        if (!computeSubmitInfos.empty())
        {
            FL_VK_ENSURE_RESULT(vkQueueSubmit(
                Context::Queues().Queue(GPUWorkloadType::Compute),
                static_cast<u32>(computeSubmitInfos.size()),
                computeSubmitInfos.data(),
                Context::Queues().QueueFence(GPUWorkloadType::Compute)
            ));
        }
        if (!transferSubmitInfos.empty())
        {
            FL_VK_ENSURE_RESULT(vkQueueSubmit(
                Context::Queues().Queue(GPUWorkloadType::Transfer),
                static_cast<u32>(transferSubmitInfos.size()),
                transferSubmitInfos.data(),
                Context::Queues().QueueFence(GPUWorkloadType::Transfer)
            ));
        }
        
        if (!m_PresentingContexts.empty())
        {
            VkPresentInfoKHR presentInfo{};
            presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
            presentInfo.waitSemaphoreCount = 1;
            presentInfo.pWaitSemaphores = finalSignalSemaphores.data();
            presentInfo.swapchainCount = presentingSwapchains.size();
            presentInfo.pSwapchains = presentingSwapchains.data();
            presentInfo.pImageIndices = presentingImageIndices.data();
        
            vkQueuePresentKHR(Context::Queues().PresentQueue(), &presentInfo);
        }
        
        m_PresentingContexts.clear();
    }

    void SubmissionHandler::PresentRenderContext(const RenderContext* context, int dependencySubmissionId)
    {
        m_PresentingContextsLock.lock();
        m_PresentingContexts.emplace_back(context, dependencySubmissionId);
        m_PresentingContextsLock.unlock();
    }

    VkSemaphore SubmissionHandler::GetTimelineSemaphore()
    {
        auto& pool = m_TimelineSemaphorePools[Flourish::Context::FrameIndex()];
        if (pool.FreeIndex >= pool.Semaphores.size())
            pool.Semaphores.push_back(Synchronization::CreateTimelineSemaphore(0));
        
        return pool.Semaphores[pool.FreeIndex++];
    }
    
    VkSemaphore SubmissionHandler::GetSemaphore()
    {
        auto& pool = m_SemaphorePools[Flourish::Context::FrameIndex()];
        if (pool.FreeIndex >= pool.Semaphores.size())
            pool.Semaphores.push_back(Synchronization::CreateSemaphore());
        
        return pool.Semaphores[pool.FreeIndex++];
    }
}