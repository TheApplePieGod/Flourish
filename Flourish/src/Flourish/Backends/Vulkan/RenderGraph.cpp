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
        FL_PROFILE_FUNCTION();

        m_ExecuteData.SubmissionOrder.clear();
        m_ExecuteData.SubmissionSyncs.clear();
        m_ExecuteData.EventData.clear();
        m_ExecuteData.SubmitData.clear();
        m_AllResources.clear();
        m_VisitedNodes.clear();
        for (u32 i = 0; i < m_SyncObjectCount; i++)
            m_ExecuteData.CompletionSemaphores[i].clear();

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

            m_ExecuteData.SubmissionOrder.emplace_back(nodeId);
        }

        m_Built = true;
        if (m_ExecuteData.SubmissionOrder.empty())
            return;
        
        // Good estimate
        m_ExecuteData.SubmissionSyncs.reserve(m_ExecuteData.SubmissionOrder.size() * 5);

        u32 totalIndex = 0;
        int currentWorkloadIndex = -1;
        GPUWorkloadType currentWorkloadType;
        for (int orderIndex = m_ExecuteData.SubmissionOrder.size() - 1; orderIndex >= 0; orderIndex--)
        {
            Node& node = m_Nodes[m_ExecuteData.SubmissionOrder[orderIndex]];
            CommandBuffer* buffer = static_cast<CommandBuffer*>(node.Buffer);
            auto& submissions = buffer->GetEncoderSubmissions();
            for (u32 subIndex = 0; subIndex < submissions.size(); subIndex++)
            {
                m_ExecuteData.SubmissionSyncs.emplace_back();
                auto& submission = submissions[subIndex];
                bool firstSub = orderIndex == m_ExecuteData.SubmissionOrder.size() - 1 && subIndex == 0;
                bool workloadChange = !firstSub && submission.AllocInfo.WorkloadType != currentWorkloadType;
                if (workloadChange || firstSub)
                {
                    m_ExecuteData.SubmissionSyncs[totalIndex].SubmitDataIndex = m_ExecuteData.SubmitData.size();
                    auto& submitData = m_ExecuteData.SubmitData.emplace_back();
                    submitData.Workload = submission.AllocInfo.WorkloadType;

                    auto& timelineSubmitInfo = submitData.TimelineSubmitInfo;
                    timelineSubmitInfo.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
                    timelineSubmitInfo.signalSemaphoreValueCount = 1;

                    for (u32 i = 0; i < m_SyncObjectCount; i++)
                    {
                        auto& submitInfo = submitData.SubmitInfos[i];
                        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
                        submitInfo.commandBufferCount = 1;
                        submitInfo.signalSemaphoreCount = 1;

                        submitData.SignalSemaphores[i] = Synchronization::CreateTimelineSemaphore(0);
                    }

                    currentWorkloadIndex = totalIndex;
                    currentWorkloadType = submitData.Workload;
                }

                for (u64 read : submission.ReadResources)
                {
                    auto resource = m_AllResources.find(read);
                    if (resource == m_AllResources.end())
                        continue;
                    auto& resourceInfo = resource->second;
                    if (resourceInfo.LastWriteIndex == -1)
                        continue;
                    if (resourceInfo.WorkloadsWaited[(u32)submission.AllocInfo.WorkloadType])
                        continue;

                    resourceInfo.WorkloadsWaited[(u32)submission.AllocInfo.WorkloadType] = true;

                    if (resourceInfo.LastWriteWorkload == submission.AllocInfo.WorkloadType)
                    {
                        // If the write occured before the previous event, we don't have to wait again
                        // because the event existing implies a read that occured before this one
                        auto& currentSync = m_ExecuteData.SubmissionSyncs[totalIndex];
                        if (resourceInfo.LastWriteIndex > currentSync.LastWaitWriteIndex)
                        {
                            u32 eventDataIndex = m_ExecuteData.EventData.size();
                            m_ExecuteData.EventData.push_back({ resourceInfo.WriteEvents, GENERIC_DEP_INFO });

                            switch (submission.AllocInfo.WorkloadType)
                            {
                                case GPUWorkloadType::Graphics:
                                { m_ExecuteData.EventData.back().DepInfo.pMemoryBarriers = &GRAPHICS_MEM_BARRIER; } break;
                                case GPUWorkloadType::Compute:
                                { m_ExecuteData.EventData.back().DepInfo.pMemoryBarriers = &COMPUTE_MEM_BARRIER; } break;
                                case GPUWorkloadType::Transfer:
                                { m_ExecuteData.EventData.back().DepInfo.pMemoryBarriers = &TRANSFER_MEM_BARRIER; } break;
                            }

                            currentSync.WaitEvents.emplace_back(eventDataIndex);
                            currentSync.LastWaitWriteIndex = resourceInfo.LastWriteIndex;

                            // Write only on the last time we wrote which will sync all writes before
                            // Should always only signal one event
                            auto& lastWriteSync = m_ExecuteData.SubmissionSyncs[resourceInfo.LastWriteIndex];
                            FL_ASSERT(lastWriteSync.WriteEvent == -1);
                            lastWriteSync.WriteEvent = eventDataIndex;
                        }
                    }
                    else
                    {
                        // If workload types are the same, we need to ensure this command buffer waits
                        // on the one where the write occured
                        // This also implies that currentWorkloadIndex != lastWorkloadIndex != -1

                        auto& fromSubmit = m_ExecuteData.SubmitData[m_ExecuteData.SubmissionSyncs[resourceInfo.LastWriteWorkloadIndex].SubmitDataIndex];
                        auto& toSubmit = m_ExecuteData.SubmitData[m_ExecuteData.SubmissionSyncs[currentWorkloadIndex].SubmitDataIndex];

                        fromSubmit.IsCompletion = false;

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

                for (u64 write : submission.WriteResources)
                {
                    // We can lazy insert once we see a write because the resouce only
                    // matters after the first write
                    auto& resource = m_AllResources[write];
                    if (resource.LastWriteIndex == -1)
                        for (u32 i = 0; i < m_SyncObjectCount; i++)
                            resource.WriteEvents[i] = Synchronization::CreateEvent();

                    resource.LastWriteIndex = totalIndex;
                    resource.LastWriteWorkloadIndex = currentWorkloadIndex;
                    resource.LastWriteWorkload = currentWorkloadType;
                    resource.WorkloadsWaited = {};
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
                    m_ExecuteData.CompletionSemaphores[i].emplace_back(info.SignalSemaphores[i]);
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

    void RenderGraph::PrepareForSubmission()
    {
        m_CurrentSemaphoreValue++;

        for (u32 i = 0; i < m_ExecuteData.WaitSemaphoreValues.size(); i++)
            m_ExecuteData.WaitSemaphoreValues[i] = m_CurrentSemaphoreValue;
        for (u32 i = 0; i < m_ExecuteData.SubmitData.size(); i++)
            m_ExecuteData.SubmitData[i].SignalSemaphoreValue = m_CurrentSemaphoreValue;
    }
}
