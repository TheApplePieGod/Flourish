#pragma once

#include "Flourish/Api/CommandBuffer.h"
#include "Flourish/Backends/Vulkan/Util/Common.h"

namespace Flourish::Vulkan
{
    struct CommandPools
    {
        VkCommandPool GraphicsPool = nullptr;
        VkCommandPool ComputePool = nullptr;
        VkCommandPool TransferPool = nullptr;

        VkCommandPool GetPool(GPUWorkloadType workloadType);
    };

    struct PersistentPools
    {
        CommandPools Pools;
        
        std::mutex Mutex;
        bool InUse = true;
        std::vector<VkCommandBuffer> GraphicsToFree;
        std::vector<VkCommandBuffer> ComputeToFree;
        std::vector<VkCommandBuffer> TransferToFree;

        void PushBufferToFree(GPUWorkloadType workloadType, VkCommandBuffer buffer);
    };
    
    struct FramePools
    {
        CommandPools Pools;

        std::vector<VkCommandBuffer> FreeGraphics;
        std::vector<VkCommandBuffer> FreeCompute;
        std::vector<VkCommandBuffer> FreeTransfer;
        u32 FreeGraphicsPtr = 0;
        u32 FreeComputePtr = 0;
        u32 FreeTransferPtr = 0;

        u64 LastAllocationFrame = 0;

        void GetBuffers(GPUWorkloadType workloadType, VkCommandBuffer* buffers, u32 bufferCount);
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
    struct CommandBufferEncoderSubmission
    {
        std::vector<VkCommandBuffer> Buffers;
        CommandBufferAllocInfo AllocInfo;
        std::unordered_set<u64> ReadResources;
        std::unordered_set<u64> WriteResources;
        Framebuffer* Framebuffer = nullptr;
    };

    class Commands
    {
    public:
        void Initialize();
        void Shutdown();

        // TS
        VkCommandPool GetPersistentPool(GPUWorkloadType workloadType);
        VkCommandPool GetFramePool(GPUWorkloadType workloadType);
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
