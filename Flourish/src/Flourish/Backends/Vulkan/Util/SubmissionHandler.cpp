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
        u32 submissionStartIndex = 0;
        u32 completionSemaphoresStartIndex = 0;
        u32 completionSemaphoresWaitCount = 0;

        std::vector<VkSubmitInfo> graphicsSubmitInfos;
        std::vector<VkSubmitInfo> computeSubmitInfos;
        std::vector<VkSubmitInfo> transferSubmitInfos;

        std::vector<VkSemaphore> completionSemaphores;
        completionSemaphores.reserve(150);
        std::vector<u64> completionSemaphoreValues;
        completionSemaphoreValues.reserve(150);
        std::vector<VkPipelineStageFlags> completionWaitStages;
        completionWaitStages.reserve(150);

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
                        subData.FirstSubmitInfo->pWaitSemaphores = completionSemaphores.data() + completionSemaphoresStartIndex;
                        subData.FirstSubmitInfo->pWaitDstStageMask = completionWaitStages.data() + completionSemaphoresStartIndex;
                        subData.TimelineSubmitInfos[0].waitSemaphoreValueCount = completionSemaphoresWaitCount;
                        subData.TimelineSubmitInfos[0].pWaitSemaphoreValues = completionSemaphoreValues.data() + completionSemaphoresStartIndex;
                    }
                    
                    // Add final sub buffer semaphore to completion list for later awaiting
                    completionSemaphores.push_back(subData.SyncSemaphores[Flourish::Context::FrameIndex()]);
                    completionSemaphoreValues.push_back(subData.SyncSemaphoreValues.back());
                    completionWaitStages.push_back(subData.FinalSubBufferWaitStage);
                    
                    // Copy submission info
                    graphicsSubmitInfos.insert(graphicsSubmitInfos.end(), subData.GraphicsSubmitInfos.begin(), subData.GraphicsSubmitInfos.end());
                    computeSubmitInfos.insert(computeSubmitInfos.end(), subData.ComputeSubmitInfos.begin(), subData.ComputeSubmitInfos.end());
                    transferSubmitInfos.insert(transferSubmitInfos.end(), subData.TransferSubmitInfos.begin(), subData.TransferSubmitInfos.end());
                }

                // Move completion pointer so that next batch will wait on semaphores from last batch
                completionSemaphoresStartIndex += completionSemaphoresWaitCount;
                completionSemaphoresWaitCount = completionSemaphores.size() - completionSemaphoresStartIndex;
            }
            
            submissionStartIndex += submissionCount;
        }

        // We need to do an initial loop over contexts to ensure that the graphics gets submitted before we present otherwise
        // vulkan will not be happy
        for (auto& contextSubmission : m_PresentingContexts)
            graphicsSubmitInfos.push_back(contextSubmission.Context->GetSubmissionData().SubmitInfo);
        
        if (!graphicsSubmitInfos.empty())
        {
            Context::Queues().ResetQueueFence(GPUWorkloadType::Graphics);
            FL_VK_ENSURE_RESULT(vkQueueSubmit(
                Context::Queues().Queue(GPUWorkloadType::Graphics),
                static_cast<u32>(graphicsSubmitInfos.size()),
                graphicsSubmitInfos.data(),
                Context::Queues().QueueFence(GPUWorkloadType::Graphics)
            ));
        }
        if (!computeSubmitInfos.empty())
        {
            Context::Queues().ResetQueueFence(GPUWorkloadType::Compute);
            FL_VK_ENSURE_RESULT(vkQueueSubmit(
                Context::Queues().Queue(GPUWorkloadType::Compute),
                static_cast<u32>(computeSubmitInfos.size()),
                computeSubmitInfos.data(),
                Context::Queues().QueueFence(GPUWorkloadType::Compute)
            ));
        }
        if (!transferSubmitInfos.empty())
        {
            Context::Queues().ResetQueueFence(GPUWorkloadType::Transfer);
            FL_VK_ENSURE_RESULT(vkQueueSubmit(
                Context::Queues().Queue(GPUWorkloadType::Transfer),
                static_cast<u32>(transferSubmitInfos.size()),
                transferSubmitInfos.data(),
                Context::Queues().QueueFence(GPUWorkloadType::Transfer)
            ));
        }

        for (auto& contextSubmission : m_PresentingContexts)
        {
            VkSwapchainKHR swapchain[1] = { contextSubmission.Context->Swapchain().GetSwapchain() };
            u32 imageIndex[1] = { contextSubmission.Context->Swapchain().GetActiveImageIndex() };

            VkPresentInfoKHR presentInfo{};
            presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
            presentInfo.waitSemaphoreCount = 1;
            presentInfo.pWaitSemaphores = &contextSubmission.Context->GetSubmissionData().SignalSemaphores[Flourish::Context::FrameIndex()];
            presentInfo.swapchainCount = 1;
            presentInfo.pSwapchains = swapchain;
            presentInfo.pImageIndices = imageIndex;
        
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