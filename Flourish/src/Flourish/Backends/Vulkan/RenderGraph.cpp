#include "flpch.h"
#include "RenderGraph.h"

#include "Flourish/Backends/Vulkan/RenderContext.h"
#include "Flourish/Backends/Vulkan/CommandBuffer.h"
#include "Flourish/Backends/Vulkan/Context.h"
#include "Flourish/Backends/Vulkan/Util/Synchronization.h"

namespace Flourish::Vulkan
{
    const SubmissionBarrier GRAPHICS_BARRIER = {
        true,
        { VK_STRUCTURE_TYPE_MEMORY_BARRIER },
        VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
        VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT
    };

    const SubmissionBarrier COMPUTE_BARRIER = {
        true,
        { VK_STRUCTURE_TYPE_MEMORY_BARRIER },
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
    };

    const SubmissionBarrier COMPUTE_BARRIER_RT = {
        true,
        { VK_STRUCTURE_TYPE_MEMORY_BARRIER },
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR
    };

    const SubmissionBarrier TRANSFER_BARRIER = {
        true,
        { VK_STRUCTURE_TYPE_MEMORY_BARRIER },
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT
    };

    RenderGraph::RenderGraph(const RenderGraphCreateInfo& createInfo)
        : Flourish::RenderGraph(createInfo)
    {
        if (createInfo.Usage == RenderGraphUsageType::PerFrame)
            m_SyncObjectCount = Flourish::Context::FrameBufferCount();
    }

    RenderGraph::~RenderGraph()
    {
        auto semaphores = m_AllSemaphores;
        Context::FinalizerQueue().Push([=]()
        {
            for (VkSemaphore sem : semaphores)
                vkDestroySemaphore(Context::Devices().Device(), sem, nullptr);
        }, "RenderGraph free");
    }

    void RenderGraph::Build()
    {
        FL_PROFILE_FUNCTION();

        m_LastWaitWrites = { -1, -1, -1 };
        m_ExecuteData.SubmissionOrder.clear();
        m_ExecuteData.SubmissionSyncs.clear();
        m_ExecuteData.SubmitData.clear();
        m_TemporarySubmissionOrder.clear();
        m_AllResources.clear();
        m_VisitedNodes.clear();
        for (u32 i = 0; i < m_SyncObjectCount; i++)
        {
            m_ExecuteData.CompletionSemaphores[i].clear();
            m_ExecuteData.CompletionFences[i].clear();
        }

        for (auto& id : m_Leaves)
            m_ProcessingNodes.emplace(id);

        // Produce initial bfs ordering
        u32 totalSubmissions = 0;
        while (!m_ProcessingNodes.empty())
        {
            u64 nodeId = m_ProcessingNodes.front();
            m_ProcessingNodes.pop();

            auto& node = m_Nodes[nodeId];
            for (u64 depId : node.ExecutionDependencies)
                m_ProcessingNodes.emplace(depId);

            m_TemporarySubmissionOrder.emplace_back(nodeId);
        }

        // Traverse the ordering backwards removing duplicates to produce
        // final topo order
        for (int i = m_TemporarySubmissionOrder.size() - 1; i >= 0; i--)
        {
            u64 id = m_TemporarySubmissionOrder[i];
            if (m_VisitedNodes.find(id) != m_VisitedNodes.end())
                continue;
            m_VisitedNodes.emplace(id);
            m_ExecuteData.SubmissionOrder.emplace_back(id);
        }

        m_Built = true;
        m_LastBuildFrame = Flourish::Context::FrameCount();
        if (m_ExecuteData.SubmissionOrder.empty())
            return;
        
        // Good estimate
        m_ExecuteData.SubmissionSyncs.reserve(m_ExecuteData.SubmissionOrder.size() * 5);

        const SubmissionBarrier& computeBarrier = Flourish::Context::FeatureTable().RayTracing
            ? COMPUTE_BARRIER_RT
            : COMPUTE_BARRIER;

        u32 semaphoreIndex = 0;
        u32 fenceIndex = 0;
        u32 eventIndex = 0;
        u32 totalIndex = 0;
        int currentWorkloadIndex = -1;
        GPUWorkloadType currentWorkloadType;
        for (u32 orderIndex = 0; orderIndex < m_ExecuteData.SubmissionOrder.size(); orderIndex++)
        {
            RenderGraphNode& node = m_Nodes[m_ExecuteData.SubmissionOrder[orderIndex]];
            CommandBuffer* buffer = static_cast<CommandBuffer*>(node.Buffer);
            for (u32 subIndex = 0; subIndex < node.EncoderNodes.size(); subIndex++)
            {
                m_ExecuteData.SubmissionSyncs.emplace_back();
                auto& submission = node.EncoderNodes[subIndex];
                u32 queueIndex = Context::Queues().QueueIndex(submission.WorkloadType);
                bool firstSub = orderIndex == 0 && subIndex == 0;
                bool queueChange = !firstSub && queueIndex != Context::Queues().QueueIndex(currentWorkloadType);
                if (queueChange || firstSub)
                {
                    m_ExecuteData.SubmissionSyncs[totalIndex].SubmitDataIndex = m_ExecuteData.SubmitData.size();
                    auto& submitData = m_ExecuteData.SubmitData.emplace_back();
                    submitData.Workload = submission.WorkloadType;

                    auto& timelineSubmitInfo = submitData.TimelineSubmitInfo;
                    timelineSubmitInfo.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
                    timelineSubmitInfo.signalSemaphoreValueCount = 1;

                    for (u32 i = 0; i < m_SyncObjectCount; i++)
                    {
                        auto& submitInfo = submitData.SubmitInfos[i];
                        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
                        submitInfo.commandBufferCount = 1;
                        submitInfo.signalSemaphoreCount = 1;

                        if (fenceIndex >= m_AllFences.size())
                            m_AllFences.emplace_back(Synchronization::CreateFence());
                        if (semaphoreIndex >= m_AllSemaphores.size())
                            m_AllSemaphores.emplace_back(Synchronization::CreateSemaphore());
                        submitData.SignalFences[i] = m_AllFences[fenceIndex++];
                        submitData.SignalSemaphores[i] = m_AllSemaphores[semaphoreIndex++];
                    }

                    currentWorkloadIndex = totalIndex;
                    currentWorkloadType = submission.WorkloadType;
                }

                auto& currentSync = m_ExecuteData.SubmissionSyncs[totalIndex];
                auto& workloadSync = m_ExecuteData.SubmissionSyncs[currentWorkloadIndex];
                for (u64 read : submission.ReadResources)
                {
                    auto resource = m_AllResources.find(read);
                    if (resource == m_AllResources.end())
                        continue;
                    auto& resourceInfo = resource->second;
                    if (resourceInfo.LastWriteIndex == -1)
                        continue;

                    u32 lastWriteQueue = Context::Queues().QueueIndex(resourceInfo.LastWriteWorkload);
                    if (lastWriteQueue == queueIndex)
                    {
                        // If the write occured before the previous wait, we don't have to wait again
                        // because the event existing implies a read that occured before this one
                        if (resourceInfo.LastWriteIndex > m_LastWaitWrites[(u32)submission.WorkloadType])
                        {
                            // Since we process reads first, always initialize the barrier
                            switch (submission.WorkloadType)
                            {
                                case GPUWorkloadType::Graphics:
                                {
                                    currentSync.Barrier = GRAPHICS_BARRIER;
                                    currentSync.Barrier.MemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
                                } break;
                                case GPUWorkloadType::Compute:
                                {
                                    currentSync.Barrier = computeBarrier;
                                    currentSync.Barrier.MemoryBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
                                } break;
                                case GPUWorkloadType::Transfer:
                                {
                                    currentSync.Barrier = TRANSFER_BARRIER;
                                    currentSync.Barrier.MemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                                } break;

                                switch (resourceInfo.LastWriteWorkload)
                                {
                                    case GPUWorkloadType::Graphics:
                                    {
                                        currentSync.Barrier.SrcStage = GRAPHICS_BARRIER.SrcStage;
                                        currentSync.Barrier.MemoryBarrier.srcAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                                    } break;
                                    case GPUWorkloadType::Compute:
                                    {
                                        currentSync.Barrier.SrcStage = computeBarrier.SrcStage;
                                        currentSync.Barrier.MemoryBarrier.srcAccessMask |= VK_ACCESS_MEMORY_WRITE_BIT;
                                    } break;
                                    case GPUWorkloadType::Transfer:
                                    {
                                        currentSync.Barrier.SrcStage = TRANSFER_BARRIER.SrcStage;
                                        currentSync.Barrier.MemoryBarrier.srcAccessMask |= VK_ACCESS_TRANSFER_WRITE_BIT;
                                    } break;
                                }
                            }

                            m_LastWaitWrites[(u32)submission.WorkloadType] = resourceInfo.LastWriteIndex;
                        }
                    }
                    else
                    {
                        // If workload types are the same, we need to ensure this command buffer waits
                        // on the one where the write occured
                        // This also implies that currentWorkloadIndex != lastWorkloadIndex != -1

                        // Check if we are already waiting on the from workload
                        int fromSubmitIndex = m_ExecuteData.SubmissionSyncs[resourceInfo.LastWriteWorkloadIndex].SubmitDataIndex;
                        auto& toSubmit = m_ExecuteData.SubmitData[workloadSync.SubmitDataIndex];
                        if (std::find(toSubmit.WaitingWorkloads.begin(), toSubmit.WaitingWorkloads.end(), fromSubmitIndex) == toSubmit.WaitingWorkloads.end())
                        {
                            auto& fromSubmit = m_ExecuteData.SubmitData[fromSubmitIndex];
                            fromSubmit.IsCompletion = false;

                            // We want to wait on the stage of the current workload since we before the
                            // execution of the stage
                            VkPipelineStageFlags waitFlags;
                            switch (submission.WorkloadType)
                            {
                                case GPUWorkloadType::Graphics:
                                { waitFlags = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT; } break;
                                case GPUWorkloadType::Compute:
                                { waitFlags = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT; } break;
                                case GPUWorkloadType::Transfer:
                                { waitFlags = VK_PIPELINE_STAGE_TRANSFER_BIT; } break;
                            }

                            toSubmit.WaitingWorkloads.emplace_back(fromSubmitIndex);
                            toSubmit.WaitStageFlags.emplace_back(waitFlags);
                            toSubmit.TimelineSubmitInfo.waitSemaphoreValueCount++;
                            for (u32 i = 0; i < m_SyncObjectCount; i++)
                            {
                                toSubmit.WaitSemaphores[i].emplace_back(fromSubmit.SignalSemaphores[i]);
                                toSubmit.SubmitInfos[i].waitSemaphoreCount++;

                                // Ensure pointers stay valid
                                toSubmit.SubmitInfos[i].pWaitSemaphores = toSubmit.WaitSemaphores[i].data();
                                toSubmit.SubmitInfos[i].pWaitDstStageMask = toSubmit.WaitStageFlags.data();
                            }
                            if (toSubmit.WaitSemaphores[0].size() > m_ExecuteData.WaitSemaphoreValues.size())
                                m_ExecuteData.WaitSemaphoreValues.emplace_back();
                        }
                    }
                }

                for (u64 write : submission.WriteResources)
                {
                    auto& resourceInfo = m_AllResources[write];

                    // Always do a write -> write sync if we are on the same workload
                    u32 lastWriteQueue = Context::Queues().QueueIndex(resourceInfo.LastWriteWorkload);
                    if (resourceInfo.LastWriteIndex != -1 && lastWriteQueue == queueIndex)
                    {
                        // Need to initialize barrier if a read has not also occured
                        switch (submission.WorkloadType)
                        {
                            case GPUWorkloadType::Graphics:
                            {
                                if (!currentSync.Barrier.ShouldBarrier)
                                    currentSync.Barrier = GRAPHICS_BARRIER;
                                currentSync.Barrier.MemoryBarrier.dstAccessMask |= VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                            } break;
                            case GPUWorkloadType::Compute:
                            {
                                if (!currentSync.Barrier.ShouldBarrier)
                                    currentSync.Barrier = computeBarrier;
                                currentSync.Barrier.MemoryBarrier.dstAccessMask |= VK_ACCESS_MEMORY_WRITE_BIT;
                            } break;
                            case GPUWorkloadType::Transfer:
                            {
                                if (!currentSync.Barrier.ShouldBarrier)
                                    currentSync.Barrier = TRANSFER_BARRIER;
                                currentSync.Barrier.MemoryBarrier.dstAccessMask |= VK_ACCESS_TRANSFER_WRITE_BIT;
                            } break;
                        }

                        switch (resourceInfo.LastWriteWorkload)
                        {
                            case GPUWorkloadType::Graphics:
                            {
                                currentSync.Barrier.SrcStage = GRAPHICS_BARRIER.SrcStage;
                                currentSync.Barrier.MemoryBarrier.srcAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                            } break;
                            case GPUWorkloadType::Compute:
                            {
                                currentSync.Barrier.SrcStage = computeBarrier.SrcStage;
                                currentSync.Barrier.MemoryBarrier.srcAccessMask |= VK_ACCESS_MEMORY_WRITE_BIT;
                            } break;
                            case GPUWorkloadType::Transfer:
                            {
                                currentSync.Barrier.SrcStage = TRANSFER_BARRIER.SrcStage;
                                currentSync.Barrier.MemoryBarrier.srcAccessMask |= VK_ACCESS_TRANSFER_WRITE_BIT;
                            } break;
                        }
                    }

                    resourceInfo.LastWriteIndex = totalIndex;
                    resourceInfo.LastWriteWorkloadIndex = currentWorkloadIndex;
                    resourceInfo.LastWriteWorkload = currentWorkloadType;
                }

                totalIndex++;
            }
        }

        // Add completion semaphores
        for (auto& info : m_ExecuteData.SubmitData)
        {
            // Need to update pointers here since SubmitData array keeps resizing
            info.TimelineSubmitInfo.pSignalSemaphoreValues = &info.SignalSemaphoreValue;
            for (u32 i = 0; i < m_SyncObjectCount; i++)
            {
                info.SubmitInfos[i].pNext = &info.TimelineSubmitInfo;
                info.SubmitInfos[i].pSignalSemaphores = &info.SignalSemaphores[i];
                if (info.IsCompletion)
                {
                    m_ExecuteData.CompletionFences[i].emplace_back(info.SignalFences[i]);
                    m_ExecuteData.CompletionSemaphores[i].emplace_back(info.SignalSemaphores[i]);
                }
            }
        }

        // Need to make sure there is enough space here because completion will
        // wait on the same values
        if (m_ExecuteData.CompletionSemaphores[0].size() > m_ExecuteData.WaitSemaphoreValues.size())
            m_ExecuteData.WaitSemaphoreValues.resize(m_ExecuteData.CompletionSemaphores[0].size());

        // Finalize submit info
        for (auto& info : m_ExecuteData.SubmitData)
            info.TimelineSubmitInfo.pWaitSemaphoreValues = m_ExecuteData.WaitSemaphoreValues.data();
    }

    // TODO: think about removing this state
    void RenderGraph::PrepareForSubmission()
    {
        if (m_LastSubmissionFrame != 0 && m_Info.Usage == RenderGraphUsageType::Once)
        {
            FL_LOG_ERROR("Cannot submit RenderGraph more than once that has usage type Once");
            throw std::exception();
        }

        if (m_LastSubmissionFrame == Flourish::Context::FrameCount())
        {
            FL_LOG_ERROR("Cannot submit RenderGraph multiple times per frame");
            throw std::exception();
        }

        if (m_Info.Usage == RenderGraphUsageType::BuildPerFrame && m_LastBuildFrame != Flourish::Context::FrameCount())
        {
            FL_LOG_ERROR("RenderGraph with usage BuildPerFrame was not built this frame");
            throw std::exception();
        }

        m_LastSubmissionFrame = Flourish::Context::FrameCount();

        m_CurrentSemaphoreValue++;

        for (u32 i = 0; i < m_ExecuteData.WaitSemaphoreValues.size(); i++)
            m_ExecuteData.WaitSemaphoreValues[i] = m_CurrentSemaphoreValue;
        for (u32 i = 0; i < m_ExecuteData.SubmitData.size(); i++)
            m_ExecuteData.SubmitData[i].SignalSemaphoreValue = m_CurrentSemaphoreValue;
    }
}
