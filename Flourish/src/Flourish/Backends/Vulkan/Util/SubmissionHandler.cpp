#include "flpch.h"
#include "SubmissionHandler.h"

#include "Flourish/Backends/Vulkan/Context.h"
#include "Flourish/Backends/Vulkan/RenderContext.h"
#include "Flourish/Backends/Vulkan/CommandBuffer.h"
#include "Flourish/Backends/Vulkan/RenderGraph.h"
#include "Flourish/Backends/Vulkan/Util/Synchronization.h"

namespace Flourish::Vulkan
{
    void SubmissionHandler::Initialize()
    {
        FL_LOG_TRACE("Vulkan submission handler initialization begin");
    }
    
    void SubmissionHandler::Shutdown()
    {

    }

    void SubmissionHandler::WaitOnFrameSemaphores()
    {
        auto& sems = m_FrameWaitSemaphores[Flourish::Context::FrameIndex()];
        auto& vals = m_FrameWaitSemaphoreValues[Flourish::Context::FrameIndex()];
        if (sems.empty()) return;

        VkSemaphoreWaitInfo waitInfo{};
        waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
        waitInfo.semaphoreCount = sems.size();
        waitInfo.pSemaphores = sems.data();
        waitInfo.pValues = vals.data();

        vkWaitSemaphores(Context::Devices().Device(), &waitInfo, UINT64_MAX);

        sems.clear();
        vals.clear();
    }

    void SubmissionHandler::ProcessFrameSubmissions()
    {
        FL_PROFILE_FUNCTION();

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

        auto& frameSems = m_FrameWaitSemaphores[Flourish::Context::FrameIndex()];
        auto& frameVals = m_FrameWaitSemaphoreValues[Flourish::Context::FrameIndex()];
        ProcessSubmission(
            Flourish::Context::FrameGraphSubmissions().data(),
            Flourish::Context::FrameGraphSubmissions().size(),
            Flourish::Context::FrameIndex(),
            &frameSems,
            &frameVals
        );

        u32 frameSemsSize = frameSems.size();
        for (Flourish::RenderContext* _context : Flourish::Context::FrameContextSubmissions())
        {
            auto context = static_cast<RenderContext*>(_context);
            if (!context->Swapchain().IsValid())
                continue;

            PresentContext(context, frameSemsSize);
        }
    }

    void SubmissionHandler::ProcessPushSubmission(Flourish::RenderGraph* graph, std::function<void()> callback)
    {
        std::vector<CommandBufferEncoderSubmission> submissionsToFree;

        /*
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
        */

        std::vector<VkSemaphore> semaphores;
        std::vector<u64> values;

        ProcessSubmission(
            &graph, 1, 0,
            &semaphores,
            &values
        );
        
        Context::FinalizerQueue().PushAsync([callback, submissionsToFree]()
        {
            for (auto& submission : submissionsToFree)
                Context::Commands().FreeBuffers(submission.AllocInfo, submission.Buffers.data(), submission.Buffers.size());

            if (callback)
                callback();
        }, &semaphores,  &values, "Push submission finalizer");
    }

    void SubmissionHandler::ProcessExecuteSubmission(Flourish::RenderGraph* graph)
    {
        std::vector<VkSemaphore> semaphores;
        std::vector<u64> values;

        ProcessSubmission(
            &graph, 1, 0,
            &semaphores,
            &values
        );
        
        VkSemaphoreWaitInfo waitInfo{};
        waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
        waitInfo.semaphoreCount = semaphores.size();
        waitInfo.pSemaphores = semaphores.data();
        waitInfo.pValues = values.data();

        vkWaitSemaphores(Context::Devices().Device(), &waitInfo, UINT64_MAX);
    }

    void SubmissionHandler::ProcessSubmission(
        Flourish::RenderGraph* const* graphs,
        u32 graphCount,
        u32 frameIndex,
        std::vector<VkSemaphore>* finalSemaphores,
        std::vector<u64>* finalSemaphoreValues)
    {
        FL_PROFILE_FUNCTION();

        VkCommandBufferBeginInfo cmdBeginInfo{};
        cmdBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        cmdBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        for (u32 graphIdx = 0; graphIdx < graphCount; graphIdx++)
        {
            auto graph = static_cast<RenderGraph*>(graphs[graphIdx]);
            graph->PrepareForSubmission();
            auto& executeData = graph->GetExecutionData();

            if (finalSemaphores && finalSemaphoreValues)
            {
                auto& sems = executeData.CompletionSemaphores[frameIndex];
                auto& vals = executeData.WaitSemaphoreValues;
                finalSemaphores->insert(finalSemaphores->end(), sems.begin(), sems.end());
                finalSemaphoreValues->insert(finalSemaphoreValues->end(), vals.begin(), vals.begin() + sems.size());
            }

            if (executeData.SubmissionOrder.empty())
                continue;

            u32 totalIndex = 0;
            int nextSubmit = -1;
            VkCommandBuffer primaryBuf;
            for (int orderIndex = executeData.SubmissionOrder.size() - 1; orderIndex >= -1; orderIndex--)
            {
                auto& node = graph->GetNode(executeData.SubmissionOrder[orderIndex == -1 ? 0 : orderIndex]);
                CommandBuffer* buffer = static_cast<CommandBuffer*>(node.Buffer);
                auto& submissions = buffer->GetEncoderSubmissions();
                for (u32 subIndex = 0; subIndex < submissions.size() || orderIndex == -1; subIndex++)
                {
                    if (orderIndex == -1 || executeData.SubmissionSyncs[totalIndex].SubmitDataIndex != -1)
                    {
                        if (nextSubmit != -1)
                        {
                            auto& submitData = executeData.SubmitData[executeData.SubmissionSyncs[nextSubmit].SubmitDataIndex];

                            vkEndCommandBuffer(primaryBuf);

                            VkSubmitInfo submitInfo = submitData.SubmitInfos[frameIndex];
                            submitInfo.pCommandBuffers = &primaryBuf;

                            Context::Queues().LockQueue(submitData.Workload, true);
                            FL_VK_ENSURE_RESULT(vkQueueSubmit(
                                Context::Queues().Queue(submitData.Workload),
                                1, &submitInfo,
                                nullptr
                            ), "Submission handler 2 submit");
                            Context::Queues().LockQueue(submitData.Workload, false);
                        }

                        if (orderIndex == -1)
                            break;

                        Context::Commands().AllocateBuffers(
                            submissions[subIndex].AllocInfo.WorkloadType,
                            false,
                            &primaryBuf, 1,
                            false
                        );   
                        
                        vkBeginCommandBuffer(primaryBuf, &cmdBeginInfo);

                        nextSubmit = totalIndex;
                    }

                    auto& syncInfo = executeData.SubmissionSyncs[totalIndex];
                    auto& submission = submissions[subIndex];

                    for (u32 waitEventIndex : syncInfo.WaitEvents)
                    {
                        auto& eventData = executeData.EventData[waitEventIndex];
                        #ifdef FL_PLATFORM_MACOS
                            VkMemoryBarrier memBarrier{};
                            memBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
                            memBarrier.srcAccessMask = eventData.DepInfo.pMemoryBarriers[0].srcAccessMask;
                            memBarrier.dstAccessMask = eventData.DepInfo.pMemoryBarriers[0].dstAccessMask;

                            vkCmdWaitEvents(
                                primaryBuf, 1,
                                &eventData.Events[frameIndex],
                                eventData.DepInfo.pMemoryBarriers[0].srcStageMask,
                                eventData.DepInfo.pMemoryBarriers[0].dstStageMask,
                                1, &memBarrier,
                                0, nullptr, 0, nullptr
                            );
                        #else
                            vkCmdWaitEvents2KHR(
                                primaryBuf, 1,
                                &eventData.Events[frameIndex],
                                &eventData.DepInfo
                            );
                        #endif
                    }

                    if (submission.Framebuffer)
                    {
                        ExecuteRenderPassCommands(
                            primaryBuf,
                            submission.Framebuffer,
                            submission.Buffers.data(),
                            submission.Buffers.size()
                        );
                    }
                    else
                        vkCmdExecuteCommands(primaryBuf, 1, &submission.Buffers[0]);

                    if (syncInfo.WriteEvent != -1)
                    {
                        auto& eventData = executeData.EventData[syncInfo.WriteEvent];
                        #ifdef FL_PLATFORM_MACOS
                            vkCmdSetEvent(
                                primaryBuf,
                                eventData.Events[frameIndex],
                                eventData.DepInfo.pMemoryBarriers[0].srcStageMask
                            );
                        #else
                            vkCmdSetEvent2KHR(
                                primaryBuf,
                                eventData.Events[frameIndex],
                                &eventData.DepInfo
                            );
                        #endif
                    }

                    totalIndex++;
                }

                buffer->ClearSubmissions();
            }
        }
    }

    void SubmissionHandler::PresentContext(RenderContext* context, u32 waitSemaphoreCount)
    {
        auto& frameSems = m_FrameWaitSemaphores[Flourish::Context::FrameIndex()];
        auto& frameVals = m_FrameWaitSemaphoreValues[Flourish::Context::FrameIndex()];
        auto& submissions = context->CommandBuffer().GetEncoderSubmissions();
        if (submissions.empty())
            throw std::exception();
        auto& submission = submissions[0];

        while (m_RenderContextWaitFlags.size() < waitSemaphoreCount)
            m_RenderContextWaitFlags.emplace_back(VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT);

        VkCommandBuffer finalBuf;
        Context::Commands().AllocateBuffers(GPUWorkloadType::Graphics, false, &finalBuf, 1, false);   
        
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(finalBuf, &beginInfo);

        auto framebuffer = context->Swapchain().GetFramebuffer();
        ExecuteRenderPassCommands(
            finalBuf,
            framebuffer,
            submission.Buffers.data(),
            submission.Buffers.size()
        );

        vkEndCommandBuffer(finalBuf);

        u64 signalSemaphoreValues[2] = { context->GetSignalValue(), 0 };
        VkTimelineSemaphoreSubmitInfo timelineSubmitInfo{};
        timelineSubmitInfo.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
        timelineSubmitInfo.signalSemaphoreValueCount = 2;
        timelineSubmitInfo.pSignalSemaphoreValues = signalSemaphoreValues;
        timelineSubmitInfo.waitSemaphoreValueCount = waitSemaphoreCount;
        timelineSubmitInfo.pWaitSemaphoreValues = frameVals.data();
    
        VkSemaphore signalSemaphores[2] = { context->GetTimelineSignalSemaphore(), context->GetBinarySignalSemaphore() };
        VkSubmitInfo finalSubmitInfo{};
        finalSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        finalSubmitInfo.pNext = &timelineSubmitInfo;
        finalSubmitInfo.commandBufferCount = 1;
        finalSubmitInfo.pCommandBuffers = &finalBuf;
        finalSubmitInfo.signalSemaphoreCount = 2;
        finalSubmitInfo.pSignalSemaphores = signalSemaphores;
        finalSubmitInfo.waitSemaphoreCount = waitSemaphoreCount;
        finalSubmitInfo.pWaitSemaphores = frameSems.data();
        finalSubmitInfo.pWaitDstStageMask = m_RenderContextWaitFlags.data();

        Context::Queues().LockQueue(GPUWorkloadType::Graphics, true);
        FL_VK_ENSURE_RESULT(vkQueueSubmit(
            Context::Queues().Queue(GPUWorkloadType::Graphics),
            1, &finalSubmitInfo,
            nullptr
        ), "Submission handler 2 submit");
        Context::Queues().LockQueue(GPUWorkloadType::Graphics, false);

        VkSwapchainKHR swapchain[1] = { context->Swapchain().GetSwapchain() };
        u32 imageIndex[1] = { context->Swapchain().GetActiveImageIndex() };

        VkSemaphore waitSemaphores[2] = { context->GetBinarySignalSemaphore(), context->GetImageAvailableSemaphore() };
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

        frameSems.emplace_back(context->GetTimelineSignalSemaphore());
        frameVals.emplace_back(context->GetSignalValue());
    }

    void SubmissionHandler::ExecuteRenderPassCommands(
        VkCommandBuffer primary,
        Framebuffer* framebuffer,
        const VkCommandBuffer* subpasses,
        u32 subpassCount
    )
    {
        VkRenderPassBeginInfo rpBeginInfo{};
        rpBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rpBeginInfo.renderPass = static_cast<RenderPass*>(framebuffer->GetRenderPass())->GetRenderPass();
        rpBeginInfo.framebuffer = framebuffer->GetFramebuffer();
        rpBeginInfo.renderArea.offset = { 0, 0 };
        rpBeginInfo.renderArea.extent = { framebuffer->GetWidth(), framebuffer->GetHeight() };
        rpBeginInfo.clearValueCount = static_cast<u32>(framebuffer->GetClearValues().size());
        rpBeginInfo.pClearValues = framebuffer->GetClearValues().data();
        vkCmdBeginRenderPass(primary, &rpBeginInfo, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);

        for (u32 subpass = 0; subpass < subpassCount; subpass++)
        {
            if (subpass != 0)
                vkCmdNextSubpass(primary, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
            vkCmdExecuteCommands(primary, 1, &subpasses[subpass]);
        }

        vkCmdEndRenderPass(primary);
    }
}
