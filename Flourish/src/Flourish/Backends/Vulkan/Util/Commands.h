#pragma once

#include "Flourish/Api/CommandBuffer.h"
#include "Flourish/Backends/Vulkan/Util/Common.h"

namespace Flourish::Vulkan
{
    struct ThreadCommandPoolsData
    {
        VkCommandPool GraphicsPool;
        VkCommandPool ComputePool;
        VkCommandPool TransferPool;
    };

    struct ThreadCommandPools
    {
        ThreadCommandPools();
        ~ThreadCommandPools();

        ThreadCommandPoolsData Data;
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
        void AllocateBuffers(
            GPUWorkloadType workloadType,
            bool secondary,
            VkCommandBuffer* buffers,
            u32 bufferCount
        );
        void FreeBuffers(
            GPUWorkloadType workloadType,
            const std::vector<VkCommandBuffer>& buffers
        );
        void FreeBuffer(
            GPUWorkloadType workloadType,
            VkCommandBuffer buffer
        );

    private:

    private:
        void DestroyPools(const ThreadCommandPoolsData& pools);

    private:
        std::unordered_map<std::thread::id, ThreadCommandPoolsData> m_PoolsInUse;
        std::vector<ThreadCommandPoolsData> m_UnusedPools;
        std::mutex m_PoolsLock;
        
        inline thread_local static ThreadCommandPools s_ThreadPools; 
    };
}