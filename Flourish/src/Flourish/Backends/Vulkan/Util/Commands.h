#pragma once

#include "Flourish/Api/CommandBuffer.h"
#include "Flourish/Backends/Vulkan/Util/Common.h"

namespace Flourish::Vulkan
{
    typedef std::array<VkCommandPool, 3> CommandPools;

    struct PersistentPools
    {
        CommandPools Pools;
        
        std::mutex Mutex;
        bool InUse = true;
        std::array<std::vector<VkCommandBuffer>, 3> BuffersToFree;

        void PushBufferToFree(GPUWorkloadType workloadType, VkCommandBuffer buffer);
    };

    struct PoolFreeList
    {
        std::vector<VkCommandBuffer> Free;
        u32 FreePtr = 0;
    };
    
    struct FramePools
    {
        CommandPools Pools;

        std::array<PoolFreeList, 3> PrimaryFreeList;
        std::array<PoolFreeList, 3> SecondaryFreeList;

        u64 LastAllocationFrame = 0;

        void GetBuffers(GPUWorkloadType workloadType, bool secondary, VkCommandBuffer* buffers, u32 bufferCount);
    };
    
    struct ThreadCommandPools
    {
        ThreadCommandPools() = default;
        ~ThreadCommandPools();

        std::shared_ptr<PersistentPools> PersistentPools;
        std::array<std::shared_ptr<FramePools>, Flourish::Context::MaxFrameBufferCount> FramePools;
    };
    
    struct CommandBufferAllocInfo
    {
        std::thread::id Thread;
        PersistentPools* PersistentPools;
        GPUWorkloadType WorkloadType;
    };

    class Framebuffer;
    class RenderContext;
    struct CommandBufferEncoderSubmission
    {
        std::vector<VkCommandBuffer> Buffers;
        CommandBufferAllocInfo AllocInfo;
        Framebuffer* Framebuffer = nullptr;
    };

    class Commands
    {
    public:
        void Initialize();
        void Shutdown();

        // TS
        VkCommandPool GetPersistentPool(GPUWorkloadType workloadType);
        void CreatePersistentPoolsForThread();
        void CreateFramePoolsForThread();
        void DestroyPoolsForThread();
        CommandBufferAllocInfo AllocateBuffers(
            GPUWorkloadType workloadType,
            bool secondary,
            VkCommandBuffer* buffers,
            u32 bufferCount,
            bool persistent
        );
        void FreeBuffers(
            const CommandBufferAllocInfo& allocInfo,
            const VkCommandBuffer* buffers,
            u32 bufferCount
        );
        void FreeBuffer(
            const CommandBufferAllocInfo& allocInfo,
            VkCommandBuffer buffer
        );

    private:
        void PopulateCommandPools(CommandPools* pools, bool allowReset);

    private:
        void DestroyPools(CommandPools* pools);
        void FreeQueuedBuffers(PersistentPools* pools);
        void FreeFrameBuffers(FramePools* pools);

    private:
        std::unordered_map<std::thread::id, ThreadCommandPools*> m_PoolsInUse;
        std::vector<std::shared_ptr<PersistentPools>> m_UnusedPersistentPools;
        std::vector<std::array<std::shared_ptr<FramePools>, Flourish::Context::MaxFrameBufferCount>> m_UnusedFramePools;
        std::mutex m_PoolsLock;
        
        inline thread_local static ThreadCommandPools s_ThreadPools; 
    };
}
