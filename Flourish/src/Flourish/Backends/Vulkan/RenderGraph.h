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
        std::array<bool, 3> WorkloadsWaited{};
        std::array<VkEvent, Flourish::Context::MaxFrameBufferCount> WriteEvents{};
    };

    struct SubmissionEventData
    {
        std::array<VkEvent, Flourish::Context::MaxFrameBufferCount> Events;
        VkDependencyInfo DepInfo;
    };

    struct SubmissionSubmitInfo
    {
        std::array<std::vector<VkSemaphore>, Flourish::Context::MaxFrameBufferCount> WaitSemaphores;
        std::array<VkSemaphore, Flourish::Context::MaxFrameBufferCount> SignalSemaphores;
        u64 SignalSemaphoreValue;
        std::vector<VkPipelineStageFlags> WaitStageFlags;
        std::array<VkSubmitInfo, Flourish::Context::MaxFrameBufferCount> SubmitInfos;
        VkTimelineSemaphoreSubmitInfo TimelineSubmitInfo;
        GPUWorkloadType Workload;
        bool IsCompletion = true;
    };

    struct SubmissionSyncInfo
    {
        int SubmitDataIndex = -1;
        int WriteEvent = -1;
        std::vector<int> WaitEvents;
        int LastWaitWriteIndex = -1;
    };

    struct GraphExecuteData
    {
        std::vector<u64> SubmissionOrder;
        std::vector<SubmissionSyncInfo> SubmissionSyncs;
        std::vector<SubmissionEventData> EventData;
        std::vector<SubmissionSubmitInfo> SubmitData;
        std::array<std::vector<VkSemaphore>, Flourish::Context::MaxFrameBufferCount> CompletionSemaphores;

        // Will be the same across all semaphores, so just needs to be
        // set to the current frame count each submit
        std::vector<u64> WaitSemaphoreValues;
    };

    class CommandBuffer;
    class RenderGraph : public Flourish::RenderGraph
    {
    public:
        RenderGraph(const RenderGraphCreateInfo& createInfo);
        ~RenderGraph() override;

        void Build() override;

        void PrepareForSubmission();

        // TS
        inline const auto& GetExecutionData() const { return m_ExecuteData; }

    private:
        GraphExecuteData m_ExecuteData;
        u64 m_CurrentSemaphoreValue = 0;

        // All resources that matter when syncing (aka writes)
        std::unordered_map<u64, ResourceSyncInfo> m_AllResources;

        // Graph traversal data
        std::queue<u64> m_ProcessingNodes;
        std::unordered_set<u64> m_VisitedNodes;

        u32 m_SyncObjectCount = 1;
    };
}
