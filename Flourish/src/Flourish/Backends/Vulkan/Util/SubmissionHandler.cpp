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
        m_SubmissionData.CompletionSemaphores.reserve(500);
        m_SubmissionData.CompletionSemaphoreValues.reserve(500);
        m_SubmissionData.CompletionWaitStages.reserve(500);
    }
    
    void SubmissionHandler::Shutdown()
    {

    }

    void SubmissionHandler::ProcessSubmissions()
    {
        u32 submissionStartIndex = 0;
        u32 completionSemaphoresStartIndex = 0;
        u32 completionSemaphoresWaitCount = 0;
        
        m_SubmissionData.GraphicsSubmitInfos.clear();
        m_SubmissionData.ComputeSubmitInfos.clear();
        m_SubmissionData.TransferSubmitInfos.clear();
        m_SubmissionData.CompletionSemaphores.clear();
        m_SubmissionData.CompletionSemaphoreValues.clear();
        m_SubmissionData.CompletionWaitStages.clear();

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
                    CommandBuffer* buffer = static_cast<CommandBuffer*>(_buffer);
                    if (buffer->GetEncoderSubmissions().empty()) continue; // TODO: warn here?

                    auto& subData = buffer->GetSubmissionData();
                    
                    // If this is not the first batch then we must wait on the previous batch to complete
                    if (completionSemaphoresWaitCount > 0)
                    {
                        subData.FirstSubmitInfo->waitSemaphoreCount = completionSemaphoresWaitCount;
                        subData.FirstSubmitInfo->pWaitSemaphores = m_SubmissionData.CompletionSemaphores.data() + completionSemaphoresStartIndex;
                        subData.FirstSubmitInfo->pWaitDstStageMask = m_SubmissionData.CompletionWaitStages.data() + completionSemaphoresStartIndex;
                        subData.TimelineSubmitInfos[0].waitSemaphoreValueCount = completionSemaphoresWaitCount;
                        subData.TimelineSubmitInfos[0].pWaitSemaphoreValues = m_SubmissionData.CompletionSemaphoreValues.data() + completionSemaphoresStartIndex;
                    }
                    
                    // Add final sub buffer semaphore to completion list for later awaiting
                    m_SubmissionData.CompletionSemaphores.push_back(subData.SyncSemaphores[Flourish::Context::FrameIndex()]);
                    m_SubmissionData.CompletionSemaphoreValues.push_back(subData.SyncSemaphoreValues.back());
                    m_SubmissionData.CompletionWaitStages.push_back(subData.FinalSubBufferWaitStage);
                    
                    // Copy submission info
                    m_SubmissionData.GraphicsSubmitInfos.insert(m_SubmissionData.GraphicsSubmitInfos.end(), subData.GraphicsSubmitInfos.begin(), subData.GraphicsSubmitInfos.end());
                    m_SubmissionData.ComputeSubmitInfos.insert(m_SubmissionData.ComputeSubmitInfos.end(), subData.ComputeSubmitInfos.begin(), subData.ComputeSubmitInfos.end());
                    m_SubmissionData.TransferSubmitInfos.insert(m_SubmissionData.TransferSubmitInfos.end(), subData.TransferSubmitInfos.begin(), subData.TransferSubmitInfos.end());
                }

                // Move completion pointer so that next batch will wait on semaphores from last batch
                completionSemaphoresStartIndex += completionSemaphoresWaitCount;
                completionSemaphoresWaitCount = m_SubmissionData.CompletionSemaphores.size() - completionSemaphoresStartIndex;
            }
            
            submissionStartIndex += submissionCount;
        }

        // We need to do an initial loop over contexts to ensure that the graphics gets submitted before we present otherwise
        // vulkan will not be happy
        for (auto context : m_PresentingContexts)
            m_SubmissionData.GraphicsSubmitInfos.push_back(context->GetSubmissionData().SubmitInfo);
        
        if (!m_SubmissionData.GraphicsSubmitInfos.empty())
        {
            Context::Queues().ResetQueueFence(GPUWorkloadType::Graphics);
            FL_VK_ENSURE_RESULT(vkQueueSubmit(
                Context::Queues().Queue(GPUWorkloadType::Graphics),
                static_cast<u32>(m_SubmissionData.GraphicsSubmitInfos.size()),
                m_SubmissionData.GraphicsSubmitInfos.data(),
                Context::Queues().QueueFence(GPUWorkloadType::Graphics)
            ));
        }
        if (!m_SubmissionData.ComputeSubmitInfos.empty())
        {
            Context::Queues().ResetQueueFence(GPUWorkloadType::Compute);
            FL_VK_ENSURE_RESULT(vkQueueSubmit(
                Context::Queues().Queue(GPUWorkloadType::Compute),
                static_cast<u32>(m_SubmissionData.ComputeSubmitInfos.size()),
                m_SubmissionData.ComputeSubmitInfos.data(),
                Context::Queues().QueueFence(GPUWorkloadType::Compute)
            ));
        }
        if (!m_SubmissionData.TransferSubmitInfos.empty())
        {
            Context::Queues().ResetQueueFence(GPUWorkloadType::Transfer);
            FL_VK_ENSURE_RESULT(vkQueueSubmit(
                Context::Queues().Queue(GPUWorkloadType::Transfer),
                static_cast<u32>(m_SubmissionData.TransferSubmitInfos.size()),
                m_SubmissionData.TransferSubmitInfos.data(),
                Context::Queues().QueueFence(GPUWorkloadType::Transfer)
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
        
            vkQueuePresentKHR(Context::Queues().PresentQueue(), &presentInfo);
        }
        
        m_PresentingContexts.clear();
    }

    void SubmissionHandler::PresentRenderContext(const RenderContext* context)
    {
        m_PresentingContextsLock.lock();
        m_PresentingContexts.push_back(context);
        m_PresentingContextsLock.unlock();
    }
}