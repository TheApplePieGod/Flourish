#include "flpch.h"
#include "SubmissionHandler.h"

#include "Flourish/Backends/Vulkan/Context.h"
#include "Flourish/Backends/Vulkan/RenderContext.h"
#include "Flourish/Backends/Vulkan/CommandBuffer.h"
#include "Flourish/Backends/Vulkan/Util/Synchronization.h"

namespace Flourish::Vulkan
{
    void SubmissionHandler::Initialize()
    {
        FL_LOG_TRACE("Vulkan submission handler initialization begin");
        
        // This will last for a very high number of submissions
        m_FrameSubmissionData.CompletionSemaphores.reserve(500);
        m_FrameSubmissionData.CompletionSemaphoreValues.reserve(500);
        m_FrameSubmissionData.CompletionWaitStages.reserve(500);
        m_PushSubmissionData.CompletionSemaphores.reserve(100);
        m_PushSubmissionData.CompletionSemaphoreValues.reserve(100);
        m_PushSubmissionData.CompletionWaitStages.reserve(100);
        m_ExecuteSubmissionData.CompletionSemaphores.reserve(100);
        m_ExecuteSubmissionData.CompletionSemaphoreValues.reserve(100);
        m_ExecuteSubmissionData.CompletionWaitStages.reserve(100);
    }
    
    void SubmissionHandler::Shutdown()
    {
        FL_LOG_TRACE("Vulkan submission handler shutdown begin");
    }

    void SubmissionHandler::WaitOnFrameSemaphores()
    {
        if (m_FrameWaitSemaphores[Flourish::Context::FrameIndex()].empty()) return;

        VkSemaphoreWaitInfo waitInfo{};
        waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
        waitInfo.semaphoreCount = m_FrameWaitSemaphores[Flourish::Context::FrameIndex()].size();
        waitInfo.pSemaphores = m_FrameWaitSemaphores[Flourish::Context::FrameIndex()].data();
        waitInfo.pValues = m_FrameWaitSemaphoreValues[Flourish::Context::FrameIndex()].data();

        vkWaitSemaphores(Context::Devices().Device(), &waitInfo, UINT64_MAX);
    }

    void SubmissionHandler::ProcessFrameSubmissions(const std::vector<std::vector<Flourish::CommandBuffer*>>& buffers, bool submit)
    {
        for (auto& list : buffers)
        {
            for (auto _buffer : list)
            {
                auto buffer = static_cast<CommandBuffer*>(_buffer);

                FL_ASSERT(buffer->IsFrameRestricted(), "Cannot include a non frame restricted command buffer in PushFrameCommandBuffers");
                FL_ASSERT(buffer->GetLastSubmitFrame() != Flourish::Context::FrameCount(), "Cannot push a command buffer twice in one frame");
                
                buffer->SetLastSubmitFrame(Flourish::Context::FrameCount());
            }
        }

        if (!submit) return;

        /*
        ProcessSubmission(
            m_FrameSubmissionData,
            Flourish::Context::FrameSubmissions().Buffers,
            &Flourish::Context::FrameSubmissions().Contexts,
            &m_FrameWaitSemaphores[Flourish::Context::FrameIndex()],
            &m_FrameWaitSemaphoreValues[Flourish::Context::FrameIndex()]
        );
        */
    }

    void SubmissionHandler::ProcessFrameSubmissions2(Flourish::CommandBuffer* const* buffers, u32 bufferCount, bool submit)
    {
        // TODO: add this check, might make more sense to do it in the final process?
        /*
        for (auto& list : buffers)
        {
            for (auto _buffer : list)
            {
                auto buffer = static_cast<CommandBuffer*>(_buffer);

                FL_ASSERT(buffer->IsFrameRestricted(), "Cannot include a non frame restricted command buffer in PushFrameCommandBuffers");
                FL_ASSERT(buffer->GetLastSubmitFrame() != Flourish::Context::FrameCount(), "Cannot push a command buffer twice in one frame");
                
                buffer->SetLastSubmitFrame(Flourish::Context::FrameCount());
            }
        }
        */

        if (!submit) return;

        //if (Flourish::Context::FrameSubmissions().Buffers.empty())
         //   return;

        ProcessSubmission2(
            m_FrameSubmissionData,
            Flourish::Context::FrameSubmissions().Buffers,
            &Flourish::Context::FrameSubmissions().Contexts,
            &m_FrameWaitSemaphores[Flourish::Context::FrameIndex()],
            &m_FrameWaitSemaphoreValues[Flourish::Context::FrameIndex()]
        );
    }

    void SubmissionHandler::ProcessPushSubmission(const std::vector<std::vector<Flourish::CommandBuffer*>>& buffers, std::function<void()> callback)
    {
        std::vector<CommandBufferEncoderSubmission> submissionsToFree;

        for (auto& list : buffers)
        {
            for (auto _buffer : list)
            {
                auto buffer = static_cast<CommandBuffer*>(_buffer);

                FL_ASSERT(!buffer->IsFrameRestricted(), "Cannot include a frame restricted command buffer in PushCommandBuffers");
                
                for (auto& submission : buffer->GetEncoderSubmissions())
                    submissionsToFree.push_back(submission);
            }
        }

        std::vector<VkSemaphore> semaphores;
        std::vector<u64> values;
        std::vector<u32> counts = { (u32)buffers.size() };

        m_PushDataMutex.lock();
        ProcessSubmission(
            m_PushSubmissionData,
            buffers,
            nullptr,
            &semaphores,
            &values
        );
        m_PushDataMutex.unlock();
        
        Context::FinalizerQueue().PushAsync([callback, submissionsToFree]()
        {
            for (auto& submission : submissionsToFree)
                Context::Commands().FreeBuffer(submission.AllocInfo, submission.Buffers[0]);

            if (callback)
                callback();
        }, &semaphores,  &values, "Push submission finalizer");

        // Clear when pushing to allow for immediate reencoding since we saved the buffers that need to
        // be freed
        for (auto& list : buffers)
            for (auto buffer : list)
                static_cast<CommandBuffer*>(buffer)->ClearSubmissions();
    }

    void SubmissionHandler::ProcessExecuteSubmission(const std::vector<std::vector<Flourish::CommandBuffer*>>& buffers)
    {
        std::vector<VkSemaphore> semaphores;
        std::vector<u64> values;
        std::vector<u32> counts = { (u32)buffers.size() };

        m_ExecuteDataMutex.lock();
        ProcessSubmission(
            m_ExecuteSubmissionData,
            buffers,
            nullptr,
            &semaphores,
            &values
        );
        m_ExecuteDataMutex.unlock();
        
        VkSemaphoreWaitInfo waitInfo{};
        waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
        waitInfo.semaphoreCount = semaphores.size();
        waitInfo.pSemaphores = semaphores.data();
        waitInfo.pValues = values.data();

        vkWaitSemaphores(Context::Devices().Device(), &waitInfo, UINT64_MAX);
    }

    VkEvent SubmissionHandler::GetNextEvent(CommandSubmissionData& submissionData)
    {
        auto& list = submissionData.EventFreeList[Flourish::Context::FrameIndex()];
        if (submissionData.EventPtr >= list.size())
            list.emplace_back(Synchronization::CreateEvent());
        return list[submissionData.EventPtr++];
    }

    VkSemaphore SubmissionHandler::GetNextSemaphore(CommandSubmissionData& submissionData)
    {
        auto& list = submissionData.SemaphoreFreeList[Flourish::Context::FrameIndex()];
        if (submissionData.SemaphorePtr >= list.size())
            list.emplace_back(Synchronization::CreateTimelineSemaphore(0));
        return list[submissionData.SemaphorePtr++];
    }

    void SubmissionHandler::ProcessSubmission2(
        CommandSubmissionData& submissionData,
        const std::vector<Flourish::CommandBuffer*>& buffers,
        const std::vector<Flourish::RenderContext*>* contexts,
        std::vector<VkSemaphore>* finalSemaphores,
        std::vector<u64>* finalSemaphoreValues)
    {
        FL_PROFILE_FUNCTION();

        if (finalSemaphores && finalSemaphoreValues)
        {
            finalSemaphores->clear();
            finalSemaphoreValues->clear();
        }

        submissionData.SemaphorePtr = 0;
        submissionData.EventPtr = 0;

        /*
         * Build submission data
         */
        
        std::vector<CommandBufferEncoderSubmission> allSubmissions;
        std::queue<CommandBuffer*> processingBuffers;
        std::unordered_set<u64> visitedBuffers;
        std::unordered_map<u64, ResourceSyncInfo> allResources;
        for (auto buf : buffers)
            processingBuffers.push(static_cast<CommandBuffer*>(buf));

        while (!processingBuffers.empty())
        {
            auto buffer = processingBuffers.front();
            processingBuffers.pop();

            u64 bufId = reinterpret_cast<u64>(buffer);
            if (visitedBuffers.find(bufId) != visitedBuffers.end())
                continue;
            visitedBuffers.insert(bufId);

            for (auto dep : buffer->GetDependencies())
                processingBuffers.push(static_cast<CommandBuffer*>(dep));

            // Insert backwards since we flip the final order
            for (int i = buffer->GetEncoderSubmissions().size() - 1; i >= 0; i--)
            {
                allSubmissions.emplace_back(buffer->GetEncoderSubmissions()[i]);
                for (u64 resource : allSubmissions.back().WriteResources)
                    allResources.insert({ resource, {} });
            }
        }

        // TODO: we could probably use a sparse syncinfo array
        std::vector<SubmissionSyncInfo> syncInfo;
        if (!allSubmissions.empty())
        {
            syncInfo.resize(allSubmissions.size());
            int currentWorkloadIndex = -1;
            for (int i = allSubmissions.size() - 1; i >= 0; i--)
            {
                auto& submission = allSubmissions[i];
                bool firstIndex = i == allSubmissions.size() - 1;
                bool workloadChange = !firstIndex && submission.AllocInfo.WorkloadType != allSubmissions[i + 1].AllocInfo.WorkloadType;
                if (workloadChange || firstIndex)
                {
                    auto& timelineSubmitInfo = syncInfo[i].TimelineSubmitInfo;
                    timelineSubmitInfo.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
                    timelineSubmitInfo.signalSemaphoreValueCount = 1;
                    timelineSubmitInfo.pSignalSemaphoreValues = syncInfo[i].SignalSemaphoreValues.data();
                    syncInfo[i].SignalSemaphoreValues[0] = Flourish::Context::FrameCount();

                    auto& submitInfo = syncInfo[i].SubmitInfo;
                    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
                    submitInfo.pNext = &timelineSubmitInfo;
                    submitInfo.commandBufferCount = 1;
                    submitInfo.signalSemaphoreCount = 1;
                    submitInfo.pSignalSemaphores = syncInfo[i].SignalSemaphores.data();

                    syncInfo[i].HasSubmit = true;
                    syncInfo[i].SignalSemaphores[0] = GetNextSemaphore(submissionData);

                    currentWorkloadIndex = i;
                }

                // If a RenderContext depends on a submission in this buffer, add
                // it to be presented with this buffer
                if (submission.RenderContext)
                {
                    auto& syncData = syncInfo[currentWorkloadIndex];
                    syncData.PresentingContexts.emplace_back(submission.RenderContext);
                    if (syncData.PresentingContexts.size() == 1)
                    {
                        // Also need to add a separate binary semaphore to wait on
                        syncData.TimelineSubmitInfo.signalSemaphoreValueCount++;
                        syncData.SubmitInfo.signalSemaphoreCount++;
                        syncData.SignalSemaphores[1] = submission.RenderContext->GetSignalSemaphore();
                    }
                }

                // TODO: we could potentially optimize this such that a wait does not occur
                // if we know we waited on a semaphore in between the write and read
                // This also won't work if there are two queues writing to the same resource
                // before it is being read
                for (u64 read : submission.ReadResources)
                {
                    auto resource = allResources.find(read);
                    if (resource == allResources.end())
                        continue;
                    auto& resourceInfo = resource->second;
                    if (resourceInfo.LastWriteIndex == -1)
                        continue;

                    if (allSubmissions[resourceInfo.LastWriteIndex].AllocInfo.WorkloadType == submission.AllocInfo.WorkloadType)
                    {
                        // If workload types are the same, we need to wait on the event
                        syncInfo[i].WaitEvents.emplace_back(resourceInfo.WriteEvent);

                        // Write only on the last time we wrote which will sync all writes before
                        syncInfo[resourceInfo.LastWriteIndex].WriteEvents.emplace_back(resourceInfo.WriteEvent);

                        VkDependencyInfo depInfo{};
                        depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
                        depInfo.memoryBarrierCount = 1;

                        VkMemoryBarrier2 barrier{};
                        barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
                        switch (submission.AllocInfo.WorkloadType)
                        {
                            case GPUWorkloadType::Graphics:
                            {
                                barrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
                                barrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
                                barrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
                                barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
                            } break;
                            case GPUWorkloadType::Compute:
                            {
                                barrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
                                barrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
                                barrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
                                barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
                            } break;
                            case GPUWorkloadType::Transfer:
                            {
                                barrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT;
                                barrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT;
                                barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
                                barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
                            } break;
                        }

                        syncInfo[i].WaitDependencies.emplace_back(depInfo);
                        syncInfo[i].WaitMemoryBarriers.emplace_back(barrier);
                        syncInfo[resourceInfo.LastWriteIndex].WriteDependencies.emplace_back(depInfo);
                        syncInfo[resourceInfo.LastWriteIndex].WriteMemoryBarriers.emplace_back(barrier);

                        // Clear the event because nothing on this queue will have to wait
                        // for this event ever again
                        resourceInfo.WriteEvent = nullptr;
                        resourceInfo.LastWriteIndex = -1;
                    }
                    else
                    {
                        // If workload types are the same, we need to ensure this command buffer waits
                        // on the one where the write occured
                        // This also implies that currentWorkloadIndex != lastWorkloadIndex != -1

                        auto& fromSync = syncInfo[resourceInfo.LastWriteWorkloadIndex];
                        auto& toSync = syncInfo[currentWorkloadIndex];

                        // We want to wait on the stage of the current workload since we before the
                        // execution of the stage
                        VkPipelineStageFlags waitFlags;
                        switch (submission.AllocInfo.WorkloadType)
                        {
                            case GPUWorkloadType::Graphics:
                            { waitFlags = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT; } break;
                            case GPUWorkloadType::Compute:
                            { waitFlags = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT; } break;
                            case GPUWorkloadType::Transfer:
                            { waitFlags = VK_PIPELINE_STAGE_TRANSFER_BIT; } break;
                        }

                        toSync.WaitSemaphores.emplace_back(fromSync.SignalSemaphores[0]);
                        toSync.WaitSemaphoreValues.emplace_back(fromSync.SignalSemaphoreValues[0]);
                        toSync.WaitStageFlags.emplace_back(waitFlags);
                    }
                }

                for (u64 write : submission.WriteResources)
                {
                    auto& resource = allResources.at(write); // Will always exist
                    if (!resource.WriteEvent)
                        resource.WriteEvent = GetNextEvent(submissionData);
                    resource.LastWriteIndex = i;
                    resource.LastWriteWorkloadIndex = currentWorkloadIndex;
                }
            }

            // Finalize sync info
            for (auto& info : syncInfo)
            {
                for (u32 j = 0; j < info.WaitDependencies.size(); j++)
                    info.WaitDependencies[j].pMemoryBarriers = &info.WaitMemoryBarriers[j];
                for (u32 j = 0; j < info.WriteDependencies.size(); j++)
                    info.WriteDependencies[j].pMemoryBarriers = &info.WriteMemoryBarriers[j];

                if (!info.HasSubmit)
                    continue;

                info.SubmitInfo.waitSemaphoreCount = info.WaitSemaphores.size();
                info.SubmitInfo.pWaitSemaphores = info.WaitSemaphores.data();
                info.SubmitInfo.pWaitDstStageMask = info.WaitStageFlags.data();
                info.TimelineSubmitInfo.waitSemaphoreValueCount = info.WaitSemaphoreValues.size();
                info.TimelineSubmitInfo.pWaitSemaphoreValues = info.WaitSemaphoreValues.data();

                if (info.WaitSemaphores.empty() && finalSemaphores && finalSemaphoreValues)
                {
                    finalSemaphores->emplace_back(info.SignalSemaphores[0]);
                    finalSemaphoreValues->emplace_back(info.SignalSemaphoreValues[0]);
                }
            }
        }

        /*
         * Execute submission data
         */

        if (!allSubmissions.empty())
        {
            int nextSubmit = -1;
            VkCommandBuffer primaryBuf;
            CommandBufferAllocInfo primaryAllocInfo;
            for (int i = allSubmissions.size() - 1; i >= -1; i--)
            {
                if (i == -1 || syncInfo[i].HasSubmit)
                {
                    if (nextSubmit != -1)
                    {
                        auto& syncData = syncInfo[nextSubmit];

                        vkEndCommandBuffer(primaryBuf);

                        Context::FinalizerQueue().Push([primaryAllocInfo, primaryBuf]()
                        {
                            Context::Commands().FreeBuffer(primaryAllocInfo, primaryBuf);
                        }, "Primarybuf finalizer");

                        syncData.SubmitInfo.pCommandBuffers = &primaryBuf;
                        GPUWorkloadType workload = allSubmissions[nextSubmit].AllocInfo.WorkloadType;
                        Context::Queues().LockQueue(workload, true);
                        FL_VK_ENSURE_RESULT(vkQueueSubmit(
                            Context::Queues().Queue(workload),
                            1, &syncData.SubmitInfo,
                            nullptr
                        ), "Submission handler 2 submit");
                        Context::Queues().LockQueue(workload, false);

                        for (RenderContext* context : syncData.PresentingContexts)
                        {
                            VkSwapchainKHR swapchain[1] = { context->Swapchain().GetSwapchain() };
                            u32 imageIndex[1] = { context->Swapchain().GetActiveImageIndex() };

                            VkSemaphore waitSemaphores[2] = { context->GetSignalSemaphore(), context->GetImageAvailableSemaphore() };
                            VkPresentInfoKHR presentInfo{};
                            presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
                            presentInfo.waitSemaphoreCount = 2;
                            presentInfo.pWaitSemaphores = waitSemaphores;
                            presentInfo.swapchainCount = 1;
                            presentInfo.pSwapchains = swapchain;
                            presentInfo.pImageIndices = imageIndex;
                            
                            Context::Queues().LockPresentQueue(true);
                            auto result = vkQueuePresentKHR(Context::Queues().PresentQueue(), &presentInfo);
                            Context::Queues().LockPresentQueue(false);
                            if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
                                context->Swapchain().Recreate();
                            else if (result != VK_SUCCESS)
                            {
                                FL_LOG_CRITICAL("Failed to present with error %d", result);
                                throw std::exception();
                            }
                        }
                    }

                    if (i == -1)
                        break;

                    primaryAllocInfo = Context::Commands().AllocateBuffers(
                        allSubmissions[i].AllocInfo.WorkloadType,
                        false,
                        &primaryBuf, 1,
                        true
                    );   
                    
                    VkCommandBufferBeginInfo beginInfo{};
                    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
                    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
                    vkBeginCommandBuffer(primaryBuf, &beginInfo);

                    nextSubmit = i;
                }

                auto& syncData = syncInfo[i];
                auto& submission = allSubmissions[i];

                if (!syncData.WaitEvents.empty())
                {
                    vkCmdWaitEvents2(
                        primaryBuf,
                        syncData.WaitEvents.size(),
                        syncData.WaitEvents.data(),
                        syncData.WaitDependencies.data()
                    );
                }

                if (submission.Framebuffer)
                {
                    VkRenderPassBeginInfo rpBeginInfo{};
                    rpBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
                    rpBeginInfo.renderPass = static_cast<RenderPass*>(submission.Framebuffer->GetRenderPass())->GetRenderPass();
                    rpBeginInfo.framebuffer = submission.Framebuffer->GetFramebuffer();
                    rpBeginInfo.renderArea.offset = { 0, 0 };
                    rpBeginInfo.renderArea.extent = { submission.Framebuffer->GetWidth(), submission.Framebuffer->GetHeight() };
                    rpBeginInfo.clearValueCount = static_cast<u32>(submission.Framebuffer->GetClearValues().size());
                    rpBeginInfo.pClearValues = submission.Framebuffer->GetClearValues().data();
                    vkCmdBeginRenderPass(primaryBuf, &rpBeginInfo, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
                }

                for (u32 subpass = 0; subpass < submission.Buffers.size(); subpass++)
                {
                    if (subpass != 0)
                        vkCmdNextSubpass(primaryBuf, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
                    vkCmdExecuteCommands(primaryBuf, 1, &submission.Buffers[subpass]);
                }

                if (submission.Framebuffer)
                    vkCmdEndRenderPass(primaryBuf);

                for (u32 j = 0; j < syncData.WriteEvents.size(); j++)
                {
                    vkCmdSetEvent2(
                        primaryBuf,
                        syncData.WriteEvents[j],
                        &syncData.WriteDependencies[j]
                    );
                }
            }
        }
    }

    void SubmissionHandler::ProcessSubmission(
        CommandSubmissionData& submissionData,
        const std::vector<std::vector<Flourish::CommandBuffer*>>& buffers,
        const std::vector<Flourish::RenderContext*>* contexts,
        std::vector<VkSemaphore>* finalSemaphores,
        std::vector<u64>* finalSemaphoreValues)
    {
    }
}
