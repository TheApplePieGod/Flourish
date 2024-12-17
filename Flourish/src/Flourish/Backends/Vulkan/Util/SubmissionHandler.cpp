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
        std::vector<VkSemaphore> sems;
        std::vector<u64> vals;

        ProcessSubmission(&graph, 1, false, &fences, &sems, &vals);
        
        if (!callback)
            return;

        Context::FinalizerQueue().PushAsync(callback, fences.data(), fences.size(), "Push submission finalizer");
    }

    void SubmissionHandler::ProcessExecuteSubmission(Flourish::RenderGraph* graph)
    {
        std::vector<VkFence> fences;
        std::vector<VkSemaphore> sems;
        std::vector<u64> vals;

        ProcessSubmission(&graph, 1, false, &fences, &sems, &vals);

        Synchronization::WaitForFences(fences.data(), fences.size());
    }

    void SubmissionHandler::ProcessGraph(
        RenderGraph* graph,
        bool frameScope,
        std::function<void(VkSubmitInfo&, VkTimelineSemaphoreSubmitInfo&)>&& preSubmitCallback)
    {
        auto& executeData = graph->GetExecutionData();
        u32 frameIndex = graph->GetExecutionFrameIndex();

        VkCommandBufferBeginInfo cmdBeginInfo{};
        cmdBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        cmdBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

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
            bool resetQueryPool = false;
            FL_ASSERT(
                finalIteration || submissions.size() == node.EncoderNodes.size(),
                "Command buffer submission count (%d) differs from specified size in render graph (%d)",
                submissions.size(), node.EncoderNodes.size()
            );
            for (u32 subIndex = 0; subIndex < submissions.size() || finalIteration; subIndex++)
            {
                // We want to iterate for each submission, but also once more for the 'final iteration' so that we can
                // end/submit the command buffer without having to duplicate the code

                bool shouldSubmitBuffer = finalIteration || executeData.SubmissionSyncs[totalIndex].SubmitDataIndex != -1;
                if (shouldSubmitBuffer)
                {
                    // If this is the final iteration or the current buffer is marked as submittable, we want to begin the
                    // command buffer that we will eventually submit

                    if (nextSubmit != -1)
                    {
                        // If we are currently processing a command buffer, we want to end it and submit it before processing the
                        // new one

                        auto& submitData = executeData.SubmitData[executeData.SubmissionSyncs[nextSubmit].SubmitDataIndex];

                        vkEndCommandBuffer(primaryBuf);

                        VkSubmitInfo submitInfo = submitData.SubmitInfos[frameIndex];
                        submitInfo.pCommandBuffers = &primaryBuf;
                        VkTimelineSemaphoreSubmitInfo timelineSubmitInfo = submitData.TimelineSubmitInfo;

                        // Run the pre-submit callback. This exists due to differing behavior between synchronization modes. Essentially
                        // all of the graph execution logic is identical, but how each mode handles wait and signal semaphores differ,
                        // so we allow that flexibility here
                        preSubmitCallback(submitInfo, timelineSubmitInfo);

                        if (Context::Devices().SupportsTimelines())
                            submitInfo.pNext = &timelineSubmitInfo;

                        VkFence fence = submitData.SignalFences[frameIndex];
                        Synchronization::ResetFences(&fence, 1);

                        Context::Queues().LockQueue(submitData.Workload, true);
                        FL_VK_ENSURE_RESULT(vkQueueSubmit(
                            Context::Queues().Queue(submitData.Workload),
                            1, &submitInfo, fence
                        ), "Submission handler submit");
                        Context::Queues().LockQueue(submitData.Workload, false);

                        if (!frameScope)
                        {
                            // If the buffer is not within the frame scope, we need to add a finalizer which will free this individual
                            // command buffer once the commands finish executing. Frame command buffers have their pools entirely reset at once

                            Context::FinalizerQueue().PushAsync([lastAlloc, primaryBuf]()
                            {
                                Context::Commands().FreeBuffer(lastAlloc, primaryBuf);
                            }, &fence, 1, "Submission free primary buffer");
                        }
                    }

                    if (finalIteration)
                        break;

                    // Begin the next command buffer to process. We only need to do this if there are more things to process (i.e.
                    // finalIteration is false)

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

                // Reset the command buffer's query pool before executing any of its commands. This
                // will noop if the buffer has no pools allocated. Query pool ops are not supported
                // on the transfer queue
                if (!resetQueryPool && submission.AllocInfo.WorkloadType != GPUWorkloadType::Transfer)
                {
                    buffer->ResetQueryPool(primaryBuf);
                    resetQueryPool = true;
                }

                if (syncInfo.Barrier.ShouldBarrier)
                {
                    // If we've determined a barrier should be here during the graph build process, insert it

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

                if (!submission.Buffers.empty())
                {
                    if (submission.Framebuffer)
                    {
                        // If the submission has a framebuffer, this indicates the following commands are associated with a renderpass,
                        // so we must specifically handle all of the pass/subpass logic

                        ExecuteRenderPassCommands(
                            primaryBuf,
                            submission.Framebuffer,
                            submission.Buffers.data(),
                            submission.Buffers.size()
                        );
                    }
                    else
                        // Otherwise we can just execute the commands normally
                        vkCmdExecuteCommands(primaryBuf, 1, &submission.Buffers[0]);

                    if (!frameScope)
                    {
                        // Like before, if this submission is not frame scoped, we must manually free all of the buffers. Here,
                        // we are freeing the secondary buffers rather than the primary.

                        auto& submitData = executeData.SubmitData[executeData.SubmissionSyncs[nextSubmit].SubmitDataIndex];
                        auto& submitInfo = submitData.SubmitInfos[frameIndex];

                        Context::FinalizerQueue().PushAsync([submission]()
                        {
                            Context::Commands().FreeBuffers(
                                submission.AllocInfo,
                                submission.Buffers.data(),
                                submission.Buffers.size()
                            );
                        }, &submitData.SignalFences[frameIndex], 1, "Submission free secondary buffers");
                    }
                }

                totalIndex++;
            }

            // Cleanup the submissions once we've processed them so that they cannot be re-processed
            buffer->ClearSubmissions();
        }
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

        // TODO: this whole system is not great, but for now we need all of them to be passed in
        FL_ASSERT(finalFences && finalSemaphores && finalSemaphoreValues);

        for (u32 graphIdx = 0; graphIdx < graphCount; graphIdx++)
        {
            auto graph = static_cast<RenderGraph*>(graphs[graphIdx]);
            FL_ASSERT(graph->IsBuilt(), "Cannot submit non-built graph");

            graph->PrepareForSubmission();

            auto& executeData = graph->GetExecutionData();
            u32 frameIndex = graph->GetExecutionFrameIndex();

            if (executeData.SubmissionOrder.empty())
                continue;

            // Process the submission differently depending on what kind of synchronization we support
            if (Context::Devices().SupportsTimelines())
                ProcessSingleSubmissionWithTimelines(graph, frameScope, finalFences, finalSemaphores, finalSemaphoreValues);
            else
                ProcessSingleSubmissionSequential(graph, frameScope, finalFences, finalSemaphores);

            // Insert the completion synchronization objects for this graph so that we maintain an entire list
            // of objects that need to be waited on to ensure command completion
            auto& fences = executeData.CompletionFences[frameIndex];
            auto& sems = executeData.CompletionSemaphores[frameIndex];
            auto& vals = executeData.WaitSemaphoreValues;
            finalFences->insert(finalFences->end(), fences.begin(), fences.end());
            finalSemaphores->insert(finalSemaphores->end(), sems.begin(), sems.end());
            finalSemaphoreValues->insert(finalSemaphoreValues->end(), vals.begin(), vals.end());
        }
    }

    void SubmissionHandler::ProcessSingleSubmissionSequential(
        RenderGraph* graph,
        bool frameScope,
        std::vector<VkFence>* finalFences,
        std::vector<VkSemaphore>* finalSemaphores)
    {
        FL_PROFILE_FUNCTION();

        // Process the submissions sequentially. This does not rely on timeline semaphores, which means the execution process needs
        // to be simplified immensely. Thus, every submission across all buffers and graphs will run in order, each depending on the
        // last.

        if (frameScope && finalSemaphores->empty())
        {
            // If this is the first submission of the frame, we want to insert the wait semaphores from last frame to wait on to
            // ensure no work overlaps between frames.

            u32 lastFrameIndex = Flourish::Context::LastFrameIndex();
            finalSemaphores->insert(finalSemaphores->end(), m_FrameWaitSemaphores[lastFrameIndex].begin(), m_FrameWaitSemaphores[lastFrameIndex].end());
        }

        auto waitFlags = &m_FrameWaitFlags;
        ProcessGraph(
            graph,
            frameScope,
            [finalSemaphores, waitFlags](VkSubmitInfo& submitInfo, VkTimelineSemaphoreSubmitInfo& timelineSubmitInfo) {
                if (submitInfo.waitSemaphoreCount > 0) return;

                // If this is the first submission of this graph, we want to wait on last frame or graph to finish. We do this
                // by storing any relevant semaphores inside finalSemaphores, so that we can wait on them here. Inner-graph sync
                // is already handled by RenderGraph.

                if (waitFlags->size() < finalSemaphores->size())
                    waitFlags->resize(finalSemaphores->size(), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);

                submitInfo.waitSemaphoreCount = finalSemaphores->size();
                submitInfo.pWaitSemaphores = finalSemaphores->data();
                submitInfo.pWaitDstStageMask = waitFlags->data();
            }
        );

        // Clear the previous final fences and semaphores. Since we are doing this sequential method of processing, whoever is waiting on
        // this submission does not need to wait for every single sub-submission, since we are ensuring every graph and submission is
        // sequential. Thus, we only need the sync objects from the last submission to be processed.
        finalFences->clear();
        finalSemaphores->clear();
    }

    void SubmissionHandler::ProcessSingleSubmissionWithTimelines(
        RenderGraph* graph,
        bool frameScope,
        std::vector<VkFence>* finalFences,
        std::vector<VkSemaphore>* finalSemaphores,
        std::vector<u64>* finalSemaphoreValues)
    {
        FL_PROFILE_FUNCTION();

        // Process the submissions using timeline semaphores. This allows for much more freedom in terms of how/when work is scheduled and
        // how dependencies are managed. We can submit the work directly from the graph representation, meaning the GPU has freedom to
        // schedule work as it sees fit, and we do not rely on any sequential ordering guarantees besides the ones explicitly defined in the graph.

        auto waitFlags = &m_FrameWaitFlags;
        auto frameSems = &m_FrameWaitSemaphores;
        auto frameVals = &m_FrameWaitSemaphoreValues;
        ProcessGraph(
            graph,
            frameScope,
            [frameScope, waitFlags, frameSems, frameVals]
            (VkSubmitInfo& submitInfo, VkTimelineSemaphoreSubmitInfo& timelineSubmitInfo) {
                if (!frameScope || submitInfo.waitSemaphoreCount > 0) return;

                // If this submission is a root node of the graph (no waiting submissions),
                // we want to wait on the last frame to finish

                u32 lastFrameIndex = Flourish::Context::LastFrameIndex();
                auto& lastSems = frameSems->at(lastFrameIndex);
                auto& lastVals = frameVals->at(lastFrameIndex);
                
                timelineSubmitInfo.waitSemaphoreValueCount = lastVals.size();
                timelineSubmitInfo.pWaitSemaphoreValues = lastVals.data();

                if (waitFlags->size() < lastSems.size())
                    waitFlags->resize(lastSems.size(), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);

                submitInfo.waitSemaphoreCount = lastSems.size();
                submitInfo.pWaitSemaphores = lastSems.data();
                submitInfo.pWaitDstStageMask = waitFlags->data();
            }
        );
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

        // Transition layout for presentation
        Texture::TransitionImageLayout(
            context->Swapchain().GetImage(),
            VK_IMAGE_LAYOUT_GENERAL,
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            VK_IMAGE_ASPECT_COLOR_BIT,
            0, 1,
            0, 1,
            0, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_ACCESS_MEMORY_READ_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            finalBuf
        );

        vkEndCommandBuffer(finalBuf);

        // Temporarily add this since we must wait on it before drawing to the swapchain images
        frameSems.push_back(context->GetImageAvailableSemaphore());
        frameVals.push_back(0);

        while (m_RenderContextWaitFlags.size() < frameSems.size())
            m_RenderContextWaitFlags.emplace_back(VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT);

        std::array<u64, 2> signalSemaphoreValues = { context->GetSignalValue(), 0 };
        VkTimelineSemaphoreSubmitInfo timelineSubmitInfo{};
        timelineSubmitInfo.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
        timelineSubmitInfo.signalSemaphoreValueCount = signalSemaphoreValues.size();
        timelineSubmitInfo.pSignalSemaphoreValues = signalSemaphoreValues.data();
        timelineSubmitInfo.waitSemaphoreValueCount = frameVals.size();
        timelineSubmitInfo.pWaitSemaphoreValues = frameVals.data();
    
        std::array<VkSemaphore, 2> signalSemaphores = {
            context->GetRenderFinishedSignalSemaphore(),
            context->GetSwapchainSignalSemaphore()
        };
        VkSubmitInfo finalSubmitInfo{};
        finalSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        finalSubmitInfo.commandBufferCount = 1;
        finalSubmitInfo.pCommandBuffers = &finalBuf;
        finalSubmitInfo.signalSemaphoreCount = signalSemaphores.size();
        finalSubmitInfo.pSignalSemaphores = signalSemaphores.data();
        finalSubmitInfo.waitSemaphoreCount = frameSems.size();
        finalSubmitInfo.pWaitSemaphores = frameSems.data();
        finalSubmitInfo.pWaitDstStageMask = m_RenderContextWaitFlags.data();
        if (Context::Devices().SupportsTimelines())
            finalSubmitInfo.pNext = &timelineSubmitInfo;

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

        VkSemaphore waitSemaphores[1] = { context->GetSwapchainSignalSemaphore() };
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
        {
            FL_LOG_DEBUG("Swapchain %x out of date or suboptimal, marking for recreation");
            context->Swapchain().Recreate();
        }
        else if (result != VK_SUCCESS)
        {
            FL_LOG_CRITICAL("Failed to present with error %d", result);
            throw std::exception();
        }

        // Clear the previous sync objects since we already waited on them
        frameFences.clear();
        frameSems.clear();
        frameVals.clear();

        // Insert the new frontmost frame dependency, which is the final graphics submission
        frameFences.emplace_back(fence);
        frameSems.emplace_back(context->GetRenderFinishedSignalSemaphore());
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
