#pragma once

#include "Flourish/Api/RenderGraph.h"
#include "Flourish/Backends/Vulkan/Util/Common.h"
#include "Flourish/Backends/Vulkan/Util/Commands.h"

namespace Flourish::Vulkan
{
    struct ResourceSyncInfo
    {
        int LastWriteIndex = -1;
        int LastWriteWorkloadIndex = -1;
        GPUWorkloadType LastWriteWorkload;
        std::array<VkEvent, Flourish::Context::MaxFrameBufferCount> WriteEvents{};
    };

    struct SubmissionEventData
    {
        std::array<VkEvent, Flourish::Context::MaxFrameBufferCount> Events;
        VkDependencyInfo DepInfo;
    };

    class RenderContext;
    struct SubmissionSubmitInfo
    {
        std::array<std::vector<VkSemaphore>, Flourish::Context::MaxFrameBufferCount> WaitSemaphores;
        std::array<std::array<VkSemaphore, 2>, Flourish::Context::MaxFrameBufferCount> SignalSemaphores;
        std::array<u64, 2> SignalSemaphoreValues;
        std::vector<VkPipelineStageFlags> WaitStageFlags;
        std::vector<RenderContext*> PresentingContexts;
        std::array<VkSubmitInfo, Flourish::Context::MaxFrameBufferCount> SubmitInfos;
        VkTimelineSemaphoreSubmitInfo TimelineSubmitInfo;
    };

    struct SubmissionSyncInfo
    {
        int SubmitDataIndex = -1;
        std::vector<u32> WriteEvents;
        std::vector<u32> WaitEvents;
    };

    class CommandBuffer;
    class RenderGraph : public Flourish::RenderGraph
    {
    public:
        RenderGraph(const RenderGraphCreateInfo& createInfo);
        ~RenderGraph() override;

        void Build() override;

        void PrepareForSubmission();

    private:
        std::vector<u64> m_SubmissionOrder;
        std::vector<SubmissionSyncInfo> m_SubmissionSyncs;
        std::vector<SubmissionEventData> m_EventData;
        std::vector<SubmissionSubmitInfo> m_SubmitData;
        std::array<std::vector<VkSemaphore>, Flourish::Context::MaxFrameBufferCount> m_CompletionSemaphores;

        // Will be the same across all semaphores, so just needs to be
        // set to the current frame count each submit
        std::vector<u64> m_WaitSemaphoreValues;
        u64 m_CurrentSemaphoreValue = 0;

        // All resources that matter when syncing (aka writes)
        std::unordered_map<u64, ResourceSyncInfo> m_AllResources;

        // Graph traversal data
        std::queue<u64> m_ProcessingNodes;
        std::unordered_set<u64> m_VisitedNodes;

        u32 m_SyncObjectCount = 1;
    };
}
