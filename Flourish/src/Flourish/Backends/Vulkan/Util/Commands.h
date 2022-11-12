#pragma once

#include "Flourish/Api/CommandBuffer.h"
#include "Flourish/Backends/Vulkan/Util/Common.h"

namespace Flourish::Vulkan
{
    struct ThreadCommandPools
    {
        ThreadCommandPools();
        ThreadCommandPools(const ThreadCommandPools& other)
            : GraphicsPool(other.GraphicsPool),
            ComputePool(other.ComputePool),
            TransferPool(other.TransferPool)
        {}
        ~ThreadCommandPools();

        void operator=(const ThreadCommandPools& other)
        {
            GraphicsPool = other.GraphicsPool;
            ComputePool = other.ComputePool;
            TransferPool = other.TransferPool;
        }

        VkCommandPool GraphicsPool;
        VkCommandPool ComputePool;
        VkCommandPool TransferPool;
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
        void DestroyPools(const ThreadCommandPools& pools);

    private:
        std::unordered_map<std::thread::id, ThreadCommandPools> m_PoolsInUse;
        std::vector<ThreadCommandPools> m_UnusedPools;
        std::mutex m_PoolsLock;
        
        inline thread_local static ThreadCommandPools s_ThreadPools; 
    };
}