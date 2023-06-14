#include "flpch.h"
#include "RenderGraph.h"

#include "Flourish/Backends/Vulkan/RenderContext.h"
#include "Flourish/Backends/Vulkan/CommandBuffer.h"
#include "Flourish/Backends/Vulkan/Util/Synchronization.h"

namespace Flourish::Vulkan
{
    const VkDependencyInfo GENERIC_DEP_INFO = {
        VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        nullptr,
        0, 1
    };

    const VkMemoryBarrier2 GRAPHICS_MEM_BARRIER = {
        VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        nullptr,
        VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
        VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT
    };

    const VkMemoryBarrier2 COMPUTE_MEM_BARRIER = {
        VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        nullptr,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_SHADER_WRITE_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_SHADER_READ_BIT
    };

    const VkMemoryBarrier2 TRANSFER_MEM_BARRIER = {
        VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        nullptr,
        VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
        VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
        VK_ACCESS_2_TRANSFER_READ_BIT
    };

    RenderGraph::RenderGraph(const RenderGraphCreateInfo& createInfo)
        : Flourish::RenderGraph(createInfo)
    {
        if (createInfo.Usage == RenderGraphUsageType::PerFrame)
            m_SyncObjectCount = Flourish::Context::FrameBufferCount();
    }

    RenderGraph::~RenderGraph()
    {
        // TODO: cleanup semaphores & events
        // Also rebuilding should reuse semaphores
    }

    void RenderGraph::Build()
    {
        m_SubmissionOrder.clear();
        m_AllResources.clear();
        m_VisitedNodes.clear();
        m_SubmissionSyncs.clear();
        m_EventData.clear();
        m_SubmitData.clear();

        for (auto& id : m_Leaves)
            m_ProcessingNodes.emplace(id);

        u32 totalSubmissions = 0;
        while (!m_ProcessingNodes.empty())
        {
            u64 nodeId = m_ProcessingNodes.front();
            m_ProcessingNodes.pop();

            if (m_VisitedNodes.find(nodeId) != m_VisitedNodes.end())
                continue;
            m_VisitedNodes.emplace(nodeId);

            auto& node = m_Nodes[nodeId];
            for (u64 depId : node.Dependencies)
                m_ProcessingNodes.emplace(depId);
        }

        if (m_SubmissionOrder.empty())
            return;
        
        // Good estimate
        m_SubmissionSyncs.reserve(m_SubmissionOrder.size() * 5);

        u32 totalIndex = 0;
        int currentWorkloadIndex = -1;
        GPUWorkloadType currentWorkloadType;
        for (int orderIndex = m_SubmissionOrder.size() - 1; orderIndex >= 0; orderIndex--)
        {
            Node& node = m_Nodes[m_SubmissionOrder[orderIndex]];
            CommandBuffer* buffer = static_cast<CommandBuffer*>(node.Buffer);
            if (!buffer) // Must be a context submission
                buffer = &static_cast<RenderContext*>(node.Context)->CommandBuffer();
            auto& submissions = buffer->GetEncoderSubmissions();
            for (int subIndex = submissions.size() - 1; subIndex >= 0; subIndex--)
            {
                m_SubmissionSyncs.emplace_back();
                auto& submission = submissions[subIndex];
                bool firstSub = orderIndex == m_SubmissionOrder.size() - 1 && subIndex == submissions.size() - 1;
                bool workloadChange = !firstSub && submission.AllocInfo.WorkloadType != currentWorkloadType;
                if (workloadChange || firstSub)
                {
                    m_SubmissionSyncs[totalIndex].SubmitDataIndex = m_SubmitData.size();
                    auto& submitData = m_SubmitData.emplace_back();

                    auto& timelineSubmitInfo = submitData.TimelineSubmitInfo;
                    timelineSubmitInfo.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
                    timelineSubmitInfo.signalSemaphoreValueCount = 1;
                    timelineSubmitInfo.pSignalSemaphoreValues = submitData.SignalSemaphoreValues.data();

                    for (u32 i = 0; i < m_SyncObjectCount; i++)
                    {
                        auto& submitInfo = submitData.SubmitInfos[i];
                        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
                        submitInfo.pNext = &timelineSubmitInfo;
                        submitInfo.commandBufferCount = 1;
                        submitInfo.signalSemaphoreCount = 1;
                        submitInfo.pSignalSemaphores = submitData.SignalSemaphores[i].data();

                        submitData.SignalSemaphores[i][0] = Synchronization::CreateTimelineSemaphore(0);
                    }

                    currentWorkloadIndex = totalIndex;
                    currentWorkloadType = submission.AllocInfo.WorkloadType;
                }

                // TODO: we could potentially optimize this such that a wait does not occur
                // if we know we waited on a semaphore in between the write and read
                // This also won't work if there are two queues writing to the same resource
                // before it is being read
                for (u64 read : submission.ReadResources)
                {
                    auto resource = m_AllResources.find(read);
                    if (resource == m_AllResources.end())
                        continue;
                    auto& resourceInfo = resource->second;
                    if (resourceInfo.LastWriteIndex == -1)
                        continue;

                    if (resourceInfo.LastWriteWorkload == submission.AllocInfo.WorkloadType)
                    {
                        u32 eventDataIndex = m_EventData.size();
                        m_EventData.push_back({ resourceInfo.WriteEvents, GENERIC_DEP_INFO });

                        switch (submission.AllocInfo.WorkloadType)
                        {
                            case GPUWorkloadType::Graphics:
                            { m_EventData.back().DepInfo.pMemoryBarriers = &GRAPHICS_MEM_BARRIER; } break;
                            case GPUWorkloadType::Compute:
                            { m_EventData.back().DepInfo.pMemoryBarriers = &COMPUTE_MEM_BARRIER; } break;
                            case GPUWorkloadType::Transfer:
                            { m_EventData.back().DepInfo.pMemoryBarriers = &TRANSFER_MEM_BARRIER; } break;
                        }

                        // If workload types are the same, we need to wait on the event
                        m_SubmissionSyncs[totalIndex].WaitEvents.emplace_back(eventDataIndex);

                        // Write only on the last time we wrote which will sync all writes before
                        m_SubmissionSyncs[resourceInfo.LastWriteIndex].WriteEvents.emplace_back(eventDataIndex);

                        // Clear the event because nothing on this queue will have to wait
                        // for this event ever again
                        resourceInfo.LastWriteIndex = -1;
                    }
                    else
                    {
                        // If workload types are the same, we need to ensure this command buffer waits
                        // on the one where the write occured
                        // This also implies that currentWorkloadIndex != lastWorkloadIndex != -1

                        auto& fromSubmit = m_SubmitData[m_SubmissionSyncs[resourceInfo.LastWriteWorkloadIndex].SubmitDataIndex];
                        auto& toSubmit = m_SubmitData[m_SubmissionSyncs[currentWorkloadIndex].SubmitDataIndex];

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

                        toSubmit.WaitStageFlags.emplace_back(waitFlags);
                        toSubmit.TimelineSubmitInfo.waitSemaphoreValueCount++;
                        for (u32 i = 0; i < m_SyncObjectCount; i++)
                        {
                            toSubmit.WaitSemaphores[i].emplace_back(fromSubmit.SignalSemaphores[i][0]);
                            toSubmit.SubmitInfos[i].waitSemaphoreCount++;

                            // Ensure pointers stay valid
                            toSubmit.SubmitInfos[i].pWaitSemaphores = toSubmit.WaitSemaphores[i].data();
                            toSubmit.SubmitInfos[i].pWaitDstStageMask = toSubmit.WaitStageFlags.data();
                        }
                        if (toSubmit.WaitSemaphores[0].size() > m_WaitSemaphoreValues.size())
                            m_WaitSemaphoreValues.emplace_back();
                    }
                }

                for (u64 write : submission.WriteResources)
                {
                    // We can lazy insert once we see a write because the resouce only
                    // matters after the first write
                    auto resource = m_AllResources[write];
                    if (resource.LastWriteIndex == -1)
                        for (u32 i = 0; i < m_SyncObjectCount; i++)
                            resource.WriteEvents[i] = Synchronization::CreateEvent();

                    resource.LastWriteIndex = totalIndex;
                    resource.LastWriteWorkloadIndex = currentWorkloadIndex;
                    resource.LastWriteWorkload = currentWorkloadType;
                }

                totalIndex++;
            }
            
            // Add presentation
            if (node.Context)
            {
                RenderContext* context = static_cast<RenderContext*>(node.Context);
                auto& submitData = m_SubmitData[m_SubmissionSyncs[currentWorkloadIndex].SubmitDataIndex];
                submitData.PresentingContexts.emplace_back(context);
                if (submitData.PresentingContexts.size() == 1)
                {
                    // Also need to add a separate binary semaphore to wait on
                    submitData.TimelineSubmitInfo.signalSemaphoreValueCount++;
                    for (u32 i = 0; i < m_SyncObjectCount; i++)
                    {
                        submitData.SubmitInfos[i].signalSemaphoreCount++;
                        submitData.SignalSemaphores[i][1] = context->GetSignalSemaphore(i);
                    }
                }
            }
        }

        // Finalize submit info
        for (auto& info : m_SubmitData)
        {
            info.TimelineSubmitInfo.pWaitSemaphoreValues = m_WaitSemaphoreValues.data();

            for (u32 i = 0; i < m_SyncObjectCount; i++)
                if (info.WaitSemaphores[i].empty())
                    m_CompletionSemaphores[i].emplace_back(info.SignalSemaphores[i][0]);
        }

        // Need to make sure there is enough space here because completion will
        // wait on the same values
        if (m_CompletionSemaphores[0].size() > m_WaitSemaphoreValues.size())
            m_WaitSemaphoreValues.resize(m_CompletionSemaphores[0].size());

        m_Built = true;
    }

    void RenderGraph::PrepareForSubmission()
    {
        m_CurrentSemaphoreValue++;

        for (u32 i = 0; i < m_WaitSemaphoreValues.size(); i++)
            m_WaitSemaphoreValues[i] = m_CurrentSemaphoreValue;
        for (u32 i = 0; i < m_SubmitData.size(); i++)
            m_SubmitData[i].SignalSemaphoreValues[0] = m_CurrentSemaphoreValue;
    }
}
