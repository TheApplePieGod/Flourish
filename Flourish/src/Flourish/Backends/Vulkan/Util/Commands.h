#pragma once

#include "Flourish/Api/CommandBuffer.h"
#include "Flourish/Backends/Vulkan/Util/Common.h"

namespace Flourish::Vulkan
{
    struct ThreadCommandPools
    {
        ThreadCommandPools() = default;
        ThreadCommandPools(const ThreadCommandPools& other)
            : GraphicsPool(other.GraphicsPool),
              ComputePool(other.ComputePool),
              TransferPool(other.TransferPool)
        {}
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
        VkCommandPool GetPool(GPUWorkloadType workloadType, std::thread::id thread = std::this_thread::get_id());
        void CreatePoolsForThread();
        void DestroyPoolsForThread();
        void AllocateBuffers(
            GPUWorkloadType workloadType,
            bool secondary,
            VkCommandBuffer* buffers,
            u32 bufferCount,
            std::thread::id thread = std::this_thread::get_id()
        );
        void FreeBuffers(
            GPUWorkloadType workloadType,
            const VkCommandBuffer* buffers,
            u32 bufferCount,
            std::thread::id thread = std::this_thread::get_id()
        );

    private:
        void DestroyPools(const ThreadCommandPools& pools);

    private:
        std::unordered_map<std::thread::id, ThreadCommandPools> m_ThreadCommandPools;
        std::vector<ThreadCommandPools> m_UnusedPools;
        std::mutex m_ThreadCommandPoolsLock;
        std::mutex m_UnusedPoolsLock;
    };
}