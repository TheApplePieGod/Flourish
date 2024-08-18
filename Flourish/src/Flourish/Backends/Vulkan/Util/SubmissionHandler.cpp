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
        auto& fences = m_FrameWaitFences[Flourish::Context::FrameIndex()];
        auto& sems = m_FrameWaitSemaphores[Flourish::Context::FrameIndex()];
        auto& vals = m_FrameWaitSemaphoreValues[Flourish::Context::FrameIndex()];

        if (fences.empty()) return;
        
        Synchronization::WaitForFences(fences.data(), fences.size());

        fences.clear();
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

        auto& frameFences = m_FrameWaitFences[Flourish::Context::FrameIndex()];
        auto& frameSems = m_FrameWaitSemaphores[Flourish::Context::FrameIndex()];
        auto& frameVals = m_FrameWaitSemaphoreValues[Flourish::Context::FrameIndex()];
        ProcessSubmission(
            Flourish::Context::FrameGraphSubmissions().data(),
            Flourish::Context::FrameGraphSubmissions().size(),
            true,
            &frameFences,
            &frameSems,
            &frameVals
        );

        for (Flourish::RenderContext* _context : Flourish::Context::FrameContextSubmissions())
        {
            auto context = static_cast<RenderContext*>(_context);
            if (!context->Swapchain().IsValid())
                continue;

            PresentContext(context);
        }
    }

    void SubmissionHandler::ProcessPushSubmission(Flourish::RenderGraph* graph, std::function<void()> callback)
    {
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

        std::vector<VkFence> fences;

        ProcessSubmission(&graph, 1, false, &fences);
        
        if (!callback)
            return;

        Context::FinalizerQueue().PushAsync(callback, fences.data(), fences.size(), "Push submission finalizer");
    }

    void SubmissionHandler::ProcessExecuteSubmission(Flourish::RenderGraph* graph)
    {
        std::vector<VkFence> fences;

        ProcessSubmission(&graph, 1, false, &fences);

        Synchronization::WaitForFences(fences.data(), fences.size());
    }

    void SubmissionHandler::ProcessSubmission(
        Flourish::RenderGraph* const* graphs,
        u32 graphCount,
        bool frameScope,
        std::vector<VkFence>* finalFences,
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
            FL_ASSERT(graph->IsBuilt(), "Cannot submit non-built graph");
            graph->PrepareForSubmission();

            auto& executeData = graph->GetExecutionData();
            u32 frameIndex = graph->GetUsage() == RenderGraphUsageType::PerFrame ? Flourish::Context::FrameIndex() : 0;

            if (finalFences)
            {
                auto& fences = executeData.CompletionFences[frameIndex];
                finalFences->insert(finalFences->end(), fences.begin(), fences.end());
            }

            if (finalSemaphores && finalSemaphoreValues)
            {
                auto& sems = executeData.CompletionSemaphores[frameIndex];
                auto& vals = executeData.WaitSemaphoreValues;
                finalSemaphores->insert(finalSemaphores->end(), sems.begin(), sems.end());
                finalSemaphoreValues->insert(finalSemaphoreValues->end(), vals.begin(), vals.begin() + sems.size());
            }

            if (executeData.SubmissionOrder.empty())
                continue;

            bool isFirstSubmit = true;
            u32 totalIndex = 0;
            int nextSubmit = -1;
            VkCommandBuffer primaryBuf;
            CommandBufferAllocInfo lastAlloc;
            for (u32 orderIndex = 0; orderIndex <= executeData.SubmissionOrder.size(); orderIndex++)
            {
                bool finalIteration = orderIndex == executeData.SubmissionOrder.size();
                const RenderGraphNode& node = graph->GetNode(executeData.SubmissionOrder[finalIteration ? 0 : orderIndex]);
                CommandBuffer* buffer = static_cast<CommandBuffer*>(node.Buffer);
                auto& submissions = buffer->GetEncoderSubmissions();
                FL_ASSERT(
                    finalIteration || submissions.size() == node.EncoderNodes.size(),
                    "Command buffer submission count (%d) differs from specified size in render graph (%d)",
                    submissions.size(), node.EncoderNodes.size()
                );
                for (u32 subIndex = 0; subIndex < submissions.size() || finalIteration; subIndex++)
                {
                    if (finalIteration || executeData.SubmissionSyncs[totalIndex].SubmitDataIndex != -1)
                    {
                        if (nextSubmit != -1)
                        {
                            auto& submitData = executeData.SubmitData[executeData.SubmissionSyncs[nextSubmit].SubmitDataIndex];

                            vkEndCommandBuffer(primaryBuf);

                            VkSubmitInfo submitInfo = submitData.SubmitInfos[frameIndex];
                            submitInfo.pCommandBuffers = &primaryBuf;

                            // Wait on last frame to finish
                            VkTimelineSemaphoreSubmitInfo timelineSubmitInfo = submitData.TimelineSubmitInfo;
                            if (isFirstSubmit)
                            {
                                u32 lastFrameIndex = Flourish::Context::LastFrameIndex();
                                timelineSubmitInfo.waitSemaphoreValueCount = m_FrameWaitSemaphoreValues[lastFrameIndex].size();
                                timelineSubmitInfo.pWaitSemaphoreValues = m_FrameWaitSemaphoreValues[lastFrameIndex].data();

                                if (m_FrameWaitFlags.size() < timelineSubmitInfo.waitSemaphoreValueCount)
                                    m_FrameWaitFlags.resize(timelineSubmitInfo.waitSemaphoreValueCount, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);

                                submitInfo.waitSemaphoreCount = m_FrameWaitSemaphores[lastFrameIndex].size();
                                submitInfo.pWaitSemaphores = m_FrameWaitSemaphores[lastFrameIndex].data();
                                submitInfo.pWaitDstStageMask = m_FrameWaitFlags.data();

                                isFirstSubmit = false;
                            }

                            VkFence fence = submitData.SignalFences[frameIndex];
                            Synchronization::ResetFences(&fence, 1);

                            Context::Queues().LockQueue(submitData.Workload, true);
                            FL_VK_ENSURE_RESULT(vkQueueSubmit(
                                Context::Queues().Queue(submitData.Workload),
                                1, &submitInfo, fence
                            ), "Submission handler submit");
                            Context::Queues().LockQueue(submitData.Workload, false);

                            // Need to free primary buffer
                            if (!frameScope)
                            {
                                Context::FinalizerQueue().PushAsync([lastAlloc, primaryBuf]()
                                {
                                    Context::Commands().FreeBuffer(lastAlloc, primaryBuf);
                                }, &fence, 1, "Frame free primary buffer");
                            }
                        }

                        if (finalIteration)
                            break;

                        lastAlloc = Context::Commands().AllocateBuffers(
                            submissions[subIndex].AllocInfo.WorkloadType,
                            false,
                            &primaryBuf, 1,
                            !frameScope
                        );   
                        
                        vkBeginCommandBuffer(primaryBuf, &cmdBeginInfo);

                        nextSubmit = totalIndex;
                    }

                    auto& syncInfo = executeData.SubmissionSyncs[totalIndex];
                    auto& submission = submissions[subIndex];

                    FL_ASSERT(
                        submission.AllocInfo.WorkloadType == node.EncoderNodes[subIndex].WorkloadType,
                        "Command buffer submission type is different than specified in the graph"
                    );

                    if (syncInfo.Barrier.ShouldBarrier)
                    {
                        vkCmdPipelineBarrier(
                            primaryBuf,
                            syncInfo.Barrier.SrcStage,
                            syncInfo.Barrier.DstStage,
                            0,
                            1, &syncInfo.Barrier.MemoryBarrier,
                            0, nullptr,
                            0, nullptr
                        );
                    }

                    // Indicates do nothing
                    if (!submission.Buffers.empty())
                    {
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

                        // Free submitted buffers
                        if (!frameScope)
                        {
                            auto& submitData = executeData.SubmitData[executeData.SubmissionSyncs[nextSubmit].SubmitDataIndex];
                            auto& submitInfo = submitData.SubmitInfos[frameIndex];

                            Context::FinalizerQueue().PushAsync([submission]()
                            {
                                Context::Commands().FreeBuffers(
                                    submission.AllocInfo,
                                    submission.Buffers.data(),
                                    submission.Buffers.size()
                                );
                            }, &submitData.SignalFences[frameIndex], 1, "Frame free submissions");
                        }
                    }

                    totalIndex++;
                }

                buffer->ClearSubmissions();
            }
        }
    }

    void SubmissionHandler::PresentContext(RenderContext* context)
    {
        auto& frameFences = m_FrameWaitFences[Flourish::Context::FrameIndex()];
        auto& frameSems = m_FrameWaitSemaphores[Flourish::Context::FrameIndex()];
        auto& frameVals = m_FrameWaitSemaphoreValues[Flourish::Context::FrameIndex()];
        auto& submissions = context->CommandBuffer().GetEncoderSubmissions();
        if (submissions.empty())
            // TODO: revisit this
            return;
        auto& submission = submissions[0];

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

        // Will be cleared later so can mutate
        frameSems.push_back(context->GetImageAvailableSemaphore());

        while (m_RenderContextWaitFlags.size() < frameSems.size())
            m_RenderContextWaitFlags.emplace_back(VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT);
    
        VkSemaphore signalSemaphores[2] = { context->GetTimelineSignalSemaphore(), context->GetBinarySignalSemaphore() };
        VkSubmitInfo finalSubmitInfo{};
        finalSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        finalSubmitInfo.commandBufferCount = 1;
        finalSubmitInfo.pCommandBuffers = &finalBuf;
        finalSubmitInfo.signalSemaphoreCount = 2;
        finalSubmitInfo.pSignalSemaphores = signalSemaphores;
        finalSubmitInfo.waitSemaphoreCount = frameSems.size();
        finalSubmitInfo.pWaitSemaphores = frameSems.data();
        finalSubmitInfo.pWaitDstStageMask = m_RenderContextWaitFlags.data();

        VkFence fence = context->GetSignalFence();
        Synchronization::ResetFences(&fence, 1);

        Context::Queues().LockQueue(GPUWorkloadType::Graphics, true);
        FL_VK_ENSURE_RESULT(vkQueueSubmit(
            Context::Queues().Queue(GPUWorkloadType::Graphics),
            1, &finalSubmitInfo, fence
        ), "Present context graphics submit");
        Context::Queues().LockQueue(GPUWorkloadType::Graphics, false);

        VkSwapchainKHR swapchain[1] = { context->Swapchain().GetSwapchain() };
        u32 imageIndex[1] = { context->Swapchain().GetActiveImageIndex() };

        VkSemaphore waitSemaphores[1] = { context->GetBinarySignalSemaphore() };
        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
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

        frameSems.clear();
        frameVals.clear();

        frameFences.emplace_back(fence);
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
