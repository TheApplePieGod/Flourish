#pragma once

#include "Flourish/Api/CommandBuffer.h"
#include "Flourish/Backends/Vulkan/Util/Common.h"

namespace Flourish::Vulkan
{
    struct ThreadCommandPoolsData
    {
        VkCommandPool GraphicsPool = nullptr;
        VkCommandPool ComputePool = nullptr;
        VkCommandPool TransferPool = nullptr;
        
        std::mutex Mutex;
        std::vector<VkCommandBuffer> GraphicsToFree;
        std::vector<VkCommandBuffer> ComputeToFree;
        std::vector<VkCommandBuffer> TransferToFree;

        VkCommandPool GetPool(GPUWorkloadType workloadType);
        void PushBufferToFree(GPUWorkloadType workloadType, VkCommandBuffer buffer);
    };

    struct ThreadCommandPools
    {
        ThreadCommandPools();
        ~ThreadCommandPools();

        std::shared_ptr<ThreadCommandPoolsData> Data;
    };
    
    struct CommandBufferAllocInfo
    {
        ThreadCommandPoolsData* Pools;
        std::thread::id Thread;
        GPUWorkloadType WorkloadType;
    };

    class Commands
    {
    public:
        void Initialize();
        void Shutdown();

        // TS
        VkCommandPool GetPool(GPUWorkloadType workloadType);
        void CreatePoolsForThread();
        void DestroyPoolsForThread();
        [[nodiscard]] CommandBufferAllocInfo AllocateBuffers(
            GPUWorkloadType workloadType,
            bool secondary,
            VkCommandBuffer* buffers,
            u32 bufferCount
        );
        void FreeBuffers(
            const CommandBufferAllocInfo& allocInfo,
            const std::vector<VkCommandBuffer>& buffers
        );
        void FreeBuffer(
            const CommandBufferAllocInfo& allocInfo,
            VkCommandBuffer buffer
        );

    private:

    private:
        void DestroyPools(ThreadCommandPoolsData* pools);
        void FreeQueuedBuffers(ThreadCommandPoolsData* pools);

    private:
        std::unordered_map<std::thread::id, std::shared_ptr<ThreadCommandPoolsData>> m_PoolsInUse;
        std::vector<std::shared_ptr<ThreadCommandPoolsData>> m_UnusedPools;
        std::mutex m_PoolsLock;
        
        inline thread_local static ThreadCommandPools s_ThreadPools; 
    };
}