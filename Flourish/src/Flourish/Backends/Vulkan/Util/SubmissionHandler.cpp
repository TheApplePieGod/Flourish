#include "flpch.h"
#include "SubmissionHandler.h"

#include "Flourish/Backends/Vulkan/Context.h"
#include "Flourish/Backends/Vulkan/RenderContext.h"
#include "Flourish/Backends/Vulkan/CommandBuffer.h"

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

        vkWaitSemaphoresKHR(Context::Devices().Device(), &waitInfo, UINT64_MAX);
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

        ProcessSubmission(
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
                Context::Commands().FreeBuffer(submission.AllocInfo, submission.Buffer);

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

        vkWaitSemaphoresKHR(Context::Devices().Device(), &waitInfo, UINT64_MAX);
    }

    void SubmissionHandler::ProcessSubmission(
        CommandSubmissionData& submissionData,
        const std::vector<std::vector<Flourish::CommandBuffer*>>& buffers,
        const std::vector<Flourish::RenderContext*>* contexts,
        std::vector<VkSemaphore>* finalSemaphores,
        std::vector<u64>* finalSemaphoreValues)
    {
        u32 completionSemaphoresAdded = 0;
        u32 completionSemaphoresStartIndex = 0;
        u32 completionSemaphoresWaitCount = 0;

        submissionData.GraphicsSubmitInfos.clear();
        submissionData.ComputeSubmitInfos.clear();
        submissionData.TransferSubmitInfos.clear();
        submissionData.CompletionSemaphores.clear();
        submissionData.CompletionSemaphoreValues.clear();
        submissionData.CompletionWaitStages.clear();
        if (finalSemaphores && finalSemaphoreValues)
        {
            finalSemaphores->clear();
            finalSemaphoreValues->clear();
        }
        
        // Each submission executes sub submissions sequentially
        for (u32 submissionIndex = 0; submissionIndex < buffers.size(); submissionIndex++)
        {
            bool isLastSubmission = submissionIndex == buffers.size() - 1;

            // Each buffer in this sub submission executes in parallel
            auto& submission = buffers[submissionIndex];
            for (auto _buffer : submission)
            {
                CommandBuffer* buffer = static_cast<CommandBuffer*>(_buffer);
                if (buffer->GetEncoderSubmissions().empty()) continue; // TODO: warn here?

                auto& subData = buffer->GetSubmissionData();
                
                // If this is not the first batch then we must wait on the previous batch to complete
                if (completionSemaphoresWaitCount > 0)
                {
                    subData.FirstSubmitInfo->waitSemaphoreCount = completionSemaphoresWaitCount;
                    subData.FirstSubmitInfo->pWaitSemaphores = submissionData.CompletionSemaphores.data() + completionSemaphoresStartIndex;
                    subData.FirstSubmitInfo->pWaitDstStageMask = submissionData.CompletionWaitStages.data() + submissionData.CompletionWaitStages.size();
                    submissionData.CompletionWaitStages.insert(submissionData.CompletionWaitStages.end(), completionSemaphoresWaitCount, subData.FirstSubBufferWaitStage);

                    subData.TimelineSubmitInfos[0].waitSemaphoreValueCount = completionSemaphoresWaitCount;
                    subData.TimelineSubmitInfos[0].pWaitSemaphoreValues = submissionData.CompletionSemaphoreValues.data() + completionSemaphoresStartIndex;
                }
                
                // Add final sub buffer semaphore to completion list for later awaiting
                submissionData.CompletionSemaphores.push_back(subData.SyncSemaphores[Flourish::Context::FrameIndex()]);
                submissionData.CompletionSemaphoreValues.push_back(buffer->GetFinalSemaphoreValue());
                completionSemaphoresAdded++;

                // For each submission, add the final semaphores of the final sub submission to the final wait semaphores
                // so we can keep track of what needs to be waited on to ensure all processing has been completed
                if (isLastSubmission && finalSemaphores && finalSemaphoreValues)
                {
                    finalSemaphores->push_back(submissionData.CompletionSemaphores.back());
                    finalSemaphoreValues->push_back(submissionData.CompletionSemaphoreValues.back());
                }

                // This is not the best thing, but we need to ensure that work gets submitted in order so that we don't get stuck
                // in a GPU deadlock waiting on semaphores. This occurs when the device does not have three separate queues, so there
                // might be room for optimization when the device has more than one or two queues.
                for (u32 i = 0; i < subData.SubmitInfos.size(); i++)
                {
                    GPUWorkloadType workloadType = buffer->GetEncoderSubmissions()[i].AllocInfo.WorkloadType;
                    Context::Queues().LockQueue(workloadType, true);
                    FL_VK_ENSURE_RESULT(vkQueueSubmit(
                        Context::Queues().Queue(workloadType),
                        1, &subData.SubmitInfos[i], nullptr
                    ), "Submission handler initial workload submit");
                    Context::Queues().LockQueue(workloadType, false);
                }
            }

            // Move completion pointer so that next batch will wait on semaphores from last batch
            completionSemaphoresStartIndex += completionSemaphoresWaitCount;
            completionSemaphoresWaitCount = completionSemaphoresAdded;
            completionSemaphoresAdded = 0;
        }

        // Loop over presenting contexts and add append graphics submissions before we submit
        if (contexts)
        {
            for (auto& _context : *contexts)
            {
                auto context = static_cast<RenderContext*>(_context);

                // Can only be graphics so we can safely just insert everything in submitinfos
                submissionData.GraphicsSubmitInfos.insert(
                    submissionData.GraphicsSubmitInfos.end(),
                    context->CommandBuffer().GetSubmissionData().SubmitInfos.begin(),
                    context->CommandBuffer().GetSubmissionData().SubmitInfos.end()
                );
            }
        }

        if (!submissionData.GraphicsSubmitInfos.empty())
        {
            Context::Queues().LockQueue(GPUWorkloadType::Graphics, true);
            FL_VK_ENSURE_RESULT(vkQueueSubmit(
                Context::Queues().Queue(GPUWorkloadType::Graphics),
                static_cast<u32>(submissionData.GraphicsSubmitInfos.size()),
                submissionData.GraphicsSubmitInfos.data(),
                nullptr
            ), "Submission handler final graphics submit");
            Context::Queues().LockQueue(GPUWorkloadType::Graphics, false);
        }
        if (!submissionData.ComputeSubmitInfos.empty())
        {
            Context::Queues().LockQueue(GPUWorkloadType::Compute, true);
            FL_VK_ENSURE_RESULT(vkQueueSubmit(
                Context::Queues().Queue(GPUWorkloadType::Compute),
                static_cast<u32>(submissionData.ComputeSubmitInfos.size()),
                submissionData.ComputeSubmitInfos.data(),
                nullptr
            ), "Submission handler final compute submit");
            Context::Queues().LockQueue(GPUWorkloadType::Compute, false);
        }
        if (!submissionData.TransferSubmitInfos.empty())
        {
            Context::Queues().LockQueue(GPUWorkloadType::Transfer, true);
            FL_VK_ENSURE_RESULT(vkQueueSubmit(
                Context::Queues().Queue(GPUWorkloadType::Transfer),
                static_cast<u32>(submissionData.TransferSubmitInfos.size()),
                submissionData.TransferSubmitInfos.data(),
                nullptr
            ), "Submission handler final transfer submit");
            Context::Queues().LockQueue(GPUWorkloadType::Transfer, false);
        }

        if (contexts)
        {
            for (auto& _context : *contexts)
            {
                auto context = static_cast<RenderContext*>(_context);

                VkSwapchainKHR swapchain[1] = { context->Swapchain().GetSwapchain() };
                u32 imageIndex[1] = { context->Swapchain().GetActiveImageIndex() };

                auto& signalSemaphores = context->GetSubmissionData().SignalSemaphores[Flourish::Context::FrameIndex()];
                auto& signalSemaphoreValues = context->GetSubmissionData().SignalSemaphoreValues[Flourish::Context::FrameIndex()];
                VkPresentInfoKHR presentInfo{};
                presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
                presentInfo.waitSemaphoreCount = 1;
                presentInfo.pWaitSemaphores = &signalSemaphores[1];
                presentInfo.swapchainCount = 1;
                presentInfo.pSwapchains = swapchain;
                presentInfo.pImageIndices = imageIndex;

                // Add final semaphore to be waited on
                if (finalSemaphores && finalSemaphoreValues)
                {
                    finalSemaphores->push_back(signalSemaphores[0]);
                    finalSemaphoreValues->push_back(signalSemaphoreValues[0]);
                }
            
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
    }
}
