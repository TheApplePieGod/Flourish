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
        auto fences = m_AllFences;
        Context::FinalizerQueue().Push([=]()
        {
            for (VkSemaphore sem : semaphores)
                vkDestroySemaphore(Context::Devices().Device(), sem, nullptr);
            for (VkFence fence : fences)
                vkDestroyFence(Context::Devices().Device(), fence, nullptr);
        }, "RenderGraph free");
    }

    void RenderGraph::AddSubmissionDependency(int fromSubmitIndex, int toSubmitIndex)
    {
        auto& toSubmit = m_ExecuteData.SubmitData[toSubmitIndex];
        if (std::find(toSubmit.WaitingWorkloads.begin(), toSubmit.WaitingWorkloads.end(), fromSubmitIndex) == toSubmit.WaitingWorkloads.end())
        {
            // Only insert a dependency if we are not already waiting on this workload for another resource

            auto& fromSubmit = m_ExecuteData.SubmitData[fromSubmitIndex];

            // Since we depend on this, we can remove the submission's completion flag
            fromSubmit.IsCompletion = false;

            // Populate sync objects from the waiting workload
            toSubmit.WaitingWorkloads.emplace_back(fromSubmitIndex);
            toSubmit.WaitStageFlags.emplace_back(GetWorkloadStageFlags(toSubmit.Workload));
            toSubmit.TimelineSubmitInfo.waitSemaphoreValueCount++;
            for (u32 i = 0; i < m_SyncObjectCount; i++)
            {
                toSubmit.WaitSemaphores[i].emplace_back(fromSubmit.SignalSemaphores[i]);
                toSubmit.SubmitInfos[i].waitSemaphoreCount++;
            }
            if (toSubmit.WaitSemaphores[0].size() > m_ExecuteData.WaitSemaphoreValues.size())
                m_ExecuteData.WaitSemaphoreValues.emplace_back();
        }
    }

    void RenderGraph::Build()
    {
        FL_PROFILE_FUNCTION();

        // Build the graph with synchronous execution if we do not support timelines. Essentially, this
        // means each submission in the graph will depend on the last, regardless of resource dependencies.
        bool synchronous = !Context::Devices().SupportsTimelines();

        ResetBuildVariables();

        PopulateSubmissionOrder();

        m_Built = true;
        m_LastBuildFrame = Flourish::Context::FrameCount();
        if (m_ExecuteData.SubmissionOrder.empty())
            return;
        
        // Good estimate
        m_ExecuteData.SubmissionSyncs.reserve(m_ExecuteData.SubmissionOrder.size() * 5);

        u32 totalIndex = 0;
        int currentWorkloadIndex = -1;
        GPUWorkloadType currentWorkloadType;
        for (u32 orderIndex = 0; orderIndex < m_ExecuteData.SubmissionOrder.size(); orderIndex++)
        {
            RenderGraphNode& node = m_Nodes[m_ExecuteData.SubmissionOrder[orderIndex]];
            CommandBuffer* buffer = static_cast<CommandBuffer*>(node.Buffer);
            for (u32 subIndex = 0; subIndex < node.EncoderNodes.size(); subIndex++)
            {
                // Insert a sync for each submission, since we may need to barrier after each command
                m_ExecuteData.SubmissionSyncs.emplace_back();

                auto& submission = node.EncoderNodes[subIndex];
                u32 queueIndex = Context::Queues().QueueIndex(submission.WorkloadType);
                bool firstSub = orderIndex == 0 && subIndex == 0;
                bool queueChange = !firstSub && queueIndex != Context::Queues().QueueIndex(currentWorkloadType);
                if (queueChange || firstSub)
                {
                    // If the queues change or this is the first submission, we must plan on submitting this workload to the queue
                    // once we finish executing its commands

                    m_ExecuteData.SubmissionSyncs[totalIndex].SubmitDataIndex = m_ExecuteData.SubmitData.size();
                    auto& submitData = m_ExecuteData.SubmitData.emplace_back();
                    submitData.Workload = submission.WorkloadType;

                    auto& timelineSubmitInfo = submitData.TimelineSubmitInfo;
                    timelineSubmitInfo.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
                    timelineSubmitInfo.signalSemaphoreValueCount = 1;

                    // Populate sync objects for this submission
                    for (u32 i = 0; i < m_SyncObjectCount; i++)
                    {
                        auto& submitInfo = submitData.SubmitInfos[i];
                        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
                        submitInfo.commandBufferCount = 1;
                        submitInfo.signalSemaphoreCount = 1;

                        submitData.SignalFences[i] = GetFence();
                        submitData.SignalSemaphores[i] = GetSemaphore();

                        if (synchronous && m_ExecuteData.SubmitData.size() > 1)
                        {
                            // If we are synchronous and this is not the first submission, add explicit dependency on the
                            // previous submission.

                            if (i == 0)
                                submitData.WaitStageFlags.push_back(GetWorkloadStageFlags(submission.WorkloadType));

                            auto& lastSubmission = m_ExecuteData.SubmitData[m_ExecuteData.SubmitData.size() - 2];
                            lastSubmission.IsCompletion = false;

                            submitData.WaitSemaphores[i].push_back(lastSubmission.SignalSemaphores[i]);
                            submitData.SubmitInfos[i].waitSemaphoreCount++;
                        }
                    }

                    currentWorkloadIndex = totalIndex;
                    currentWorkloadType = submission.WorkloadType;
                }

                auto& currentSync = m_ExecuteData.SubmissionSyncs[totalIndex];
                auto& workloadSync = m_ExecuteData.SubmissionSyncs[currentWorkloadIndex];

                // Process all read resources and check for any dependencies that need to be resolved
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
                        // If the queues between write -> read match, we must sync via a memory barrier. However, we only
                        // need to do this if we have not already waited on a memory barrier for this same sesource (i.e.
                        // if the write occured before the last wait, we do not need to wait again)

                        bool wroteAfterLastBarrier = resourceInfo.LastWriteIndex > m_LastWaitWrites[queueIndex];
                        if (wroteAfterLastBarrier)
                        {
                            PopulateSubmissionBarrier(currentSync.Barrier, resourceInfo.LastWriteWorkload, submission.WorkloadType);
                            m_LastWaitWrites[queueIndex] = resourceInfo.LastWriteIndex;
                        }
                    }
                    else if (!synchronous)
                    {
                        // If the queues between write -> read differ, we need to ensure this command buffer waits
                        // on the one where the write occured. We only need to do this if the graph is not synchronous, since
                        // synchronous graphs already have explicit dependencies between each submission.
                        // This also implies that currentWorkloadIndex != lastWorkloadIndex != -1

                        int fromSubmitIndex = m_ExecuteData.SubmissionSyncs[resourceInfo.LastWriteWorkloadIndex].SubmitDataIndex;
                        AddSubmissionDependency(fromSubmitIndex, workloadSync.SubmitDataIndex);
                    }
                }

                // Process all write resources and check for any dependencies that need to be resolved
                for (u64 write : submission.WriteResources)
                {
                    auto& resourceInfo = m_AllResources[write];

                    u32 lastWriteQueue = Context::Queues().QueueIndex(resourceInfo.LastWriteWorkload);
                    bool wasWritten = resourceInfo.LastWriteIndex != -1;
                    if (wasWritten && lastWriteQueue == queueIndex)
                    {
                        // If the queues between write -> write match, we must sync via a memory barrier in order to prevent
                        // a write-after-write hazard. Similar to reads, we don't need to insert another barrier if we've already waited
                        // in this queue

                        bool wroteAfterLastBarrier = resourceInfo.LastWriteIndex > m_LastWaitWrites[queueIndex];
                        if (wroteAfterLastBarrier)
                        {
                            PopulateSubmissionBarrier(currentSync.Barrier, resourceInfo.LastWriteWorkload, submission.WorkloadType);
                            m_LastWaitWrites[queueIndex] = totalIndex;
                        }
                    }
                    else if (wasWritten && !synchronous)
                    {
                        // If the queues between write -> write differ, we need to ensure this command buffer waits
                        // on the one where the write occured. We only need to do this if the graph is not synchronous, since
                        // synchronous graphs already have explicit dependencies between each submission.
                        // This also implies that currentWorkloadIndex != lastWorkloadIndex != -1.
                        // We could change this behavior to only apply if there is an explicit dependency between these workloads.
                        // However, to guarantee consistency, this is the best option.

                        int fromSubmitIndex = m_ExecuteData.SubmissionSyncs[resourceInfo.LastWriteWorkloadIndex].SubmitDataIndex;
                        AddSubmissionDependency(fromSubmitIndex, workloadSync.SubmitDataIndex);
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
                info.SubmitInfos[i].pSignalSemaphores = &info.SignalSemaphores[i];
                info.SubmitInfos[i].pWaitSemaphores = info.WaitSemaphores[i].data();
                info.SubmitInfos[i].pWaitDstStageMask = info.WaitStageFlags.data();
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

        if (Context::Devices().SupportsTimelines())
        {
            // Don't bother with this if we are not using timelines

            m_CurrentSemaphoreValue++;

            for (u32 i = 0; i < m_ExecuteData.WaitSemaphoreValues.size(); i++)
                m_ExecuteData.WaitSemaphoreValues[i] = m_CurrentSemaphoreValue;
            for (u32 i = 0; i < m_ExecuteData.SubmitData.size(); i++)
                m_ExecuteData.SubmitData[i].SignalSemaphoreValue = m_CurrentSemaphoreValue;
        }
    }

    u32 RenderGraph::GetExecutionFrameIndex() const
    {
        return m_Info.Usage == RenderGraphUsageType::PerFrame ? Flourish::Context::FrameIndex() : 0;
    }

    void RenderGraph::ResetBuildVariables()
    {
        m_FreeSemaphoreIndex = 0;
        m_FreeFenceIndex = 0;

        m_LastWaitWrites.clear();
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
    }

    void RenderGraph::PopulateSubmissionOrder()
    {
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
    }

    VkPipelineStageFlags RenderGraph::GetWorkloadStageFlags(GPUWorkloadType type)
    {
        switch (type)
        {
            case GPUWorkloadType::Graphics: { return VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT; }
            case GPUWorkloadType::Compute: { return VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT; }
            case GPUWorkloadType::Transfer: { return VK_PIPELINE_STAGE_TRANSFER_BIT; }
        }
    }

    void RenderGraph::PopulateSubmissionBarrier(SubmissionBarrier& barrier, GPUWorkloadType srcWorkload, GPUWorkloadType dstWorkload)
    {
        const SubmissionBarrier& computeBarrier = Flourish::Context::FeatureTable().RayTracing
            ? COMPUTE_BARRIER_RT
            : COMPUTE_BARRIER;

        switch (dstWorkload)
        {
            case GPUWorkloadType::Graphics:
            {
                if (!barrier.ShouldBarrier)
                    barrier = GRAPHICS_BARRIER;
                barrier.MemoryBarrier.dstAccessMask |= VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            } break;
            case GPUWorkloadType::Compute:
            {
                if (!barrier.ShouldBarrier)
                    barrier = computeBarrier;
                barrier.MemoryBarrier.dstAccessMask |= VK_ACCESS_MEMORY_WRITE_BIT;
            } break;
            case GPUWorkloadType::Transfer:
            {
                if (!barrier.ShouldBarrier)
                    barrier = TRANSFER_BARRIER;
                barrier.MemoryBarrier.dstAccessMask |= VK_ACCESS_TRANSFER_WRITE_BIT;
            } break;
        }

        switch (srcWorkload)
        {
            case GPUWorkloadType::Graphics:
            {
                barrier.SrcStage = GRAPHICS_BARRIER.SrcStage;
                barrier.MemoryBarrier.srcAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            } break;
            case GPUWorkloadType::Compute:
            {
                barrier.SrcStage = computeBarrier.SrcStage;
                barrier.MemoryBarrier.srcAccessMask |= VK_ACCESS_MEMORY_WRITE_BIT;
            } break;
            case GPUWorkloadType::Transfer:
            {
                barrier.SrcStage = TRANSFER_BARRIER.SrcStage;
                barrier.MemoryBarrier.srcAccessMask |= VK_ACCESS_TRANSFER_WRITE_BIT;
            } break;
        }
    }

    VkSemaphore RenderGraph::GetSemaphore()
    {
        if (m_FreeSemaphoreIndex >= m_AllSemaphores.size())
        {
            VkSemaphore newSem = Context::Devices().SupportsTimelines()
                ? Synchronization::CreateTimelineSemaphore(0)
                : Synchronization::CreateSemaphore();
            m_AllSemaphores.emplace_back(newSem);
        }
        return m_AllSemaphores[m_FreeSemaphoreIndex++];
    }

    VkFence RenderGraph::GetFence()
    {
        if (m_FreeFenceIndex >= m_AllFences.size())
            m_AllFences.emplace_back(Synchronization::CreateFence());
        return m_AllFences[m_FreeFenceIndex++];
    }
}
