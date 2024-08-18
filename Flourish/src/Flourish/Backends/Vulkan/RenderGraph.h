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
    };

    struct SubmissionSubmitInfo
    {
        std::vector<int> WaitingWorkloads;
        std::array<std::vector<VkSemaphore>, Flourish::Context::MaxFrameBufferCount> WaitSemaphores;
        std::array<VkSemaphore, Flourish::Context::MaxFrameBufferCount> SignalSemaphores;
        std::array<VkFence, Flourish::Context::MaxFrameBufferCount> SignalFences;
        u64 SignalSemaphoreValue;
        std::vector<VkPipelineStageFlags> WaitStageFlags;
        std::array<VkSubmitInfo, Flourish::Context::MaxFrameBufferCount> SubmitInfos;
        VkTimelineSemaphoreSubmitInfo TimelineSubmitInfo;
        GPUWorkloadType Workload;
        bool IsCompletion = true;
    };

    struct SubmissionBarrier
    {
        bool ShouldBarrier = false;
        VkMemoryBarrier MemoryBarrier{};
        VkPipelineStageFlags SrcStage = 0;
        VkPipelineStageFlags DstStage = 0;
    };

    struct SubmissionSyncInfo
    {
        int SubmitDataIndex = -1;
        SubmissionBarrier Barrier;
    };

    struct GraphExecuteData
    {
        std::vector<u64> SubmissionOrder;
        std::vector<SubmissionSyncInfo> SubmissionSyncs;
        std::vector<SubmissionSubmitInfo> SubmitData;
        std::array<std::vector<VkSemaphore>, Flourish::Context::MaxFrameBufferCount> CompletionSemaphores;
        std::array<std::vector<VkFence>, Flourish::Context::MaxFrameBufferCount> CompletionFences;

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
        u32 GetExecutionFrameIndex() const;

        // TS
        inline const auto& GetExecutionData() const { return m_ExecuteData; }

    private:
        void ResetBuildVariables();
        void PopulateSubmissionOrder();
        VkPipelineStageFlags GetWorkloadStageFlags(GPUWorkloadType type);
        void PopulateSubmisssionBarrier(SubmissionBarrier& barrier, GPUWorkloadType srcWorkload, GPUWorkloadType dstWorkload);
        VkSemaphore GetSemaphore();
        VkFence GetFence();

    private:
        // Execution data relevant for each submission
        GraphExecuteData m_ExecuteData;
        u64 m_CurrentSemaphoreValue = 0;
        u64 m_LastSubmissionFrame = 0;
        u64 m_LastBuildFrame = 0;

        // Temporary build data to be reset on each build
        u32 m_FreeSemaphoreIndex = 0;
        u32 m_FreeFenceIndex = 0;
        std::vector<VkSemaphore> m_AllSemaphores;
        std::vector<VkFence> m_AllFences;
        std::unordered_map<u32, u32> m_LastWaitWrites;
        std::unordered_map<u64, ResourceSyncInfo> m_AllResources;
        std::queue<u64> m_ProcessingNodes;
        std::unordered_set<u64> m_VisitedNodes;
        std::vector<u64> m_TemporarySubmissionOrder;

        u32 m_SyncObjectCount = 1;
    };
}
