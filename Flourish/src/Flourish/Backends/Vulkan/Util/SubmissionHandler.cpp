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
            Flourish::Context::FrameSubmissions(),
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

    void SubmissionHandler::ProcessSubmission2(
        CommandSubmissionData& submissionData,
        const std::vector<Flourish::RenderGraph*>& graphs,
        std::vector<VkSemaphore>* finalSemaphores,
        std::vector<u64>* finalSemaphoreValues)
    {
        FL_PROFILE_FUNCTION();

        for (auto _graph  : graphs)
        {
            auto graph = static_cast<RenderGraph*>(_graph);
            graph->PrepareForSubmission();
            auto& executeData = graph->GetExecutionData();

            if (finalSemaphores && finalSemaphoreValues)
            {
                auto& sems = executeData.CompletionSemaphores[Flourish::Context::FrameIndex()];
                auto& vals = executeData.WaitSemaphoreValues;
                finalSemaphores->insert(finalSemaphores->end(), sems.begin(), sems.end());
                finalSemaphoreValues->insert(finalSemaphoreValues->end(), vals.begin(), vals.begin() + sems.size());
            }

            if (executeData.SubmissionOrder.empty())
                continue;

            u32 totalIndex = 0;
            int nextSubmit = -1;
            VkCommandBuffer primaryBuf;
            CommandBufferAllocInfo primaryAllocInfo;
            for (int orderIndex = executeData.SubmissionOrder.size() - 1; orderIndex >= -1; orderIndex--)
            {
                auto& node = graph->GetNode(executeData.SubmissionOrder[orderIndex == -1 ? 0 : orderIndex]);
                CommandBuffer* buffer = static_cast<CommandBuffer*>(node.Buffer);
                if (!buffer) // Must be a context submission
                    buffer = &static_cast<RenderContext*>(node.Context)->CommandBuffer();
                auto& submissions = buffer->GetEncoderSubmissions();
                for (int subIndex = submissions.size() - 1; subIndex >= 0 || orderIndex == -1; subIndex--)
                {
                    if (orderIndex == -1 || executeData.SubmissionSyncs[totalIndex].SubmitDataIndex != -1)
                    {
                        if (nextSubmit != -1)
                        {
                            auto& submitData = executeData.SubmitData[executeData.SubmissionSyncs[nextSubmit].SubmitDataIndex];

                            vkEndCommandBuffer(primaryBuf);

                            Context::FinalizerQueue().Push([primaryAllocInfo, primaryBuf]()
                            {
                                Context::Commands().FreeBuffer(primaryAllocInfo, primaryBuf);
                            }, "Primarybuf finalizer");

                            VkSubmitInfo submitInfo = submitData.SubmitInfos[Flourish::Context::FrameIndex()];
                            submitInfo.pCommandBuffers = &primaryBuf;

                            Context::Queues().LockQueue(submitData.Workload, true);
                            FL_VK_ENSURE_RESULT(vkQueueSubmit(
                                Context::Queues().Queue(submitData.Workload),
                                1, &submitInfo,
                                nullptr
                            ), "Submission handler 2 submit");
                            Context::Queues().LockQueue(submitData.Workload, false);

                            for (RenderContext* context : submitData.PresentingContexts)
                            {
                                if (!context->Swapchain().IsValid())
                                    continue;

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

                        if (orderIndex == -1)
                            break;

                        primaryAllocInfo = Context::Commands().AllocateBuffers(
                            submissions[subIndex].AllocInfo.WorkloadType,
                            false,
                            &primaryBuf, 1,
                            true
                        );   
                        
                        VkCommandBufferBeginInfo beginInfo{};
                        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
                        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
                        vkBeginCommandBuffer(primaryBuf, &beginInfo);

                        nextSubmit = totalIndex;
                    }

                    auto& syncInfo = executeData.SubmissionSyncs[totalIndex];
                    auto& submission = submissions[subIndex];

                    for (u32 waitEventIndex : syncInfo.WaitEvents)
                    {
                        auto& eventData = executeData.EventData[waitEventIndex];
                        vkCmdWaitEvents2(
                            primaryBuf, 1,
                            &eventData.Events[Flourish::Context::FrameIndex()],
                            &eventData.DepInfo
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

                    for (u32 writeEventIndex : syncInfo.WriteEvents)
                    {
                        auto& eventData = executeData.EventData[writeEventIndex];
                        vkCmdSetEvent2(
                            primaryBuf,
                            eventData.Events[Flourish::Context::FrameIndex()],
                            &eventData.DepInfo
                        );
                    }

                    totalIndex++;
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
