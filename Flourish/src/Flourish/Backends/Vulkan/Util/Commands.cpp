#include "flpch.h"
#include "Commands.h"

#include "Flourish/Backends/Vulkan/Context.h"

namespace Flourish::Vulkan
{
    VkCommandPool CommandPools::GetPool(GPUWorkloadType workloadType)
    {
        switch (workloadType)
        {
            case GPUWorkloadType::Graphics:
            { return GraphicsPool; }
            case GPUWorkloadType::Transfer:
            { return TransferPool; }
            case GPUWorkloadType::Compute:
            { return ComputePool; }
        }

        FL_ASSERT(false, "Command pool for workload not supported");
        return nullptr;
    }

    void PersistentPools::PushBufferToFree(GPUWorkloadType workloadType, VkCommandBuffer buffer)
    {
        switch (workloadType)
        {
            case GPUWorkloadType::Graphics:
            { GraphicsToFree.push_back(buffer); } return;
            case GPUWorkloadType::Transfer:
            { TransferToFree.push_back(buffer); } return;
            case GPUWorkloadType::Compute:
            { ComputeToFree.push_back(buffer); } return;
        }

        FL_ASSERT(false, "Command pool for workload not supported");
    }

    void FramePools::GetBuffers(GPUWorkloadType workloadType, VkCommandBuffer* buffers, u32 bufferCount)
    {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
        allocInfo.commandBufferCount = bufferCount;

        switch (workloadType)
        {
            case GPUWorkloadType::Graphics:
            {
                if (FreeGraphicsPtr + bufferCount > FreeGraphics.size())
                {
                    FreeGraphics.resize(FreeGraphicsPtr + bufferCount);

                    allocInfo.commandPool = Pools.GraphicsPool;
                    if (!FL_VK_CHECK_RESULT(vkAllocateCommandBuffers(
                        Context::Devices().Device(),
                        &allocInfo, FreeGraphics.data() + FreeGraphicsPtr
                    ), "Allocate frame command buffers"))
                        throw std::exception();
                }

                memcpy(buffers, FreeGraphics.data() + FreeGraphicsPtr, bufferCount * sizeof(VkCommandBuffer));
                
                FreeGraphicsPtr += bufferCount;
            } return;
            case GPUWorkloadType::Transfer:
            {
                if (FreeTransferPtr + bufferCount > FreeTransfer.size())
                {
                    FreeTransfer.resize(FreeTransferPtr + bufferCount);

                    allocInfo.commandPool = Pools.TransferPool;
                    if (!FL_VK_CHECK_RESULT(vkAllocateCommandBuffers(
                        Context::Devices().Device(),
                        &allocInfo, FreeTransfer.data() + FreeTransferPtr
                    ), "Allocate frame command buffers"))
                        throw std::exception();
                }

                memcpy(buffers, FreeTransfer.data() + FreeTransferPtr, bufferCount * sizeof(VkCommandBuffer));
                
                FreeTransferPtr += bufferCount;
            } return;
            case GPUWorkloadType::Compute:
            {
                if (FreeComputePtr + bufferCount > FreeCompute.size())
                {
                    FreeCompute.resize(FreeComputePtr + bufferCount);

                    allocInfo.commandPool = Pools.ComputePool;
                    if (!FL_VK_CHECK_RESULT(vkAllocateCommandBuffers(
                        Context::Devices().Device(),
                        &allocInfo, FreeCompute.data() + FreeComputePtr
                    ), "Allocate frame command buffers"))
                        throw std::exception();
                }

                memcpy(buffers, FreeCompute.data() + FreeComputePtr, bufferCount * sizeof(VkCommandBuffer));
                
                FreeComputePtr += bufferCount;
            } return;
        }

        FL_ASSERT(false, "Command pool for workload not supported");
    }

    ThreadCommandPools::~ThreadCommandPools()
    {
        if (!Context::Devices().Device()) return;
        Context::Commands().DestroyPoolsForThread();
    }

    void Commands::Initialize()
    {
        FL_LOG_TRACE("Vulkan commands initialization begin");
    }

    void Commands::Shutdown()
    {
        FL_LOG_TRACE("Vulkan commands shutdown begin");

        auto device = Context::Devices().Device();

        for (auto& pair : m_PoolsInUse)
        {
            if (pair.second->PersistentPools)
                DestroyPools(&pair.second->PersistentPools->Pools);
            if (pair.second->FramePools[0])
                for (u32 frame = 0; frame < Flourish::Context::FrameBufferCount(); frame++)
                    DestroyPools(&pair.second->FramePools[frame]->Pools);
        }
        for (auto& pools : m_UnusedPersistentPools)
            DestroyPools(&pools->Pools);
        for (auto& pools : m_UnusedFramePools)
            for (u32 frame = 0; frame < Flourish::Context::FrameBufferCount(); frame++)
                DestroyPools(&pools[frame]->Pools);
        
        m_PoolsInUse.clear();
        m_UnusedPersistentPools.clear();
        m_UnusedFramePools.clear();
    }

    VkCommandPool Commands::GetPersistentPool(GPUWorkloadType workloadType)
    {
        CreatePersistentPoolsForThread();
        return s_ThreadPools.PersistentPools->Pools.GetPool(workloadType);
    }

    VkCommandPool Commands::GetFramePool(GPUWorkloadType workloadType)
    {
        CreateFramePoolsForThread();
        return s_ThreadPools.FramePools[Flourish::Context::FrameIndex()]->Pools.GetPool(workloadType);
    }

    void Commands::CreatePersistentPoolsForThread()
    {
        if (s_ThreadPools.PersistentPools) return;

        auto device = Context::Devices().Device();

        m_PoolsLock.lock();
        if (m_UnusedPersistentPools.size() > 0)
        {
            s_ThreadPools.PersistentPools = m_UnusedPersistentPools.back();
            m_UnusedPersistentPools.pop_back();
            s_ThreadPools.PersistentPools->Mutex.lock();
            s_ThreadPools.PersistentPools->InUse = true;
            s_ThreadPools.PersistentPools->Mutex.unlock();
        }
        else
        {
            s_ThreadPools.PersistentPools = std::make_shared<PersistentPools>();
            PopulateCommandPools(&s_ThreadPools.PersistentPools->Pools, true);
        }

        m_PoolsInUse[std::this_thread::get_id()] = &s_ThreadPools;
        m_PoolsLock.unlock();
    }

    void Commands::CreateFramePoolsForThread()
    {
        if (s_ThreadPools.FramePools[0]) return;

        auto device = Context::Devices().Device();

        m_PoolsLock.lock();
        if (m_UnusedFramePools.size() > 0)
        {
            s_ThreadPools.FramePools = m_UnusedFramePools.back();
            m_UnusedFramePools.pop_back();
            
            // Reset entire frame pool for this current frame if it has not been allocated on yet
            auto framePool = s_ThreadPools.FramePools[Flourish::Context::FrameIndex()].get();
            FreeFrameBuffers(framePool);
        }
        else
        {
            for (u32 i = 0; i < Flourish::Context::FrameBufferCount(); i++)
            {
                s_ThreadPools.FramePools[i] = std::make_shared<FramePools>();
                s_ThreadPools.FramePools[i]->LastAllocationFrame = Flourish::Context::FrameCount();
                PopulateCommandPools(&s_ThreadPools.FramePools[i]->Pools, false);
            }
        }

        m_PoolsInUse[std::this_thread::get_id()] = &s_ThreadPools;
        m_PoolsLock.unlock();
    }

    void Commands::DestroyPoolsForThread()
    {
        m_PoolsLock.lock();

        if (s_ThreadPools.PersistentPools)
        {
            m_UnusedPersistentPools.push_back(s_ThreadPools.PersistentPools);
            FreeQueuedBuffers(s_ThreadPools.PersistentPools.get());
            s_ThreadPools.PersistentPools->Mutex.lock();
            s_ThreadPools.PersistentPools->InUse = false;
            s_ThreadPools.PersistentPools->Mutex.unlock();
        }
        if (s_ThreadPools.FramePools[0])
            m_UnusedFramePools.push_back(s_ThreadPools.FramePools);

        m_PoolsInUse.erase(std::this_thread::get_id());

        m_PoolsLock.unlock();
    }

    CommandBufferAllocInfo Commands::AllocateBuffers(GPUWorkloadType workloadType, bool secondary, VkCommandBuffer* buffers, u32 bufferCount, bool persistent)
    {
        FL_ASSERT(persistent || secondary, "Cannot allocate a primary non-persistent buffer");

        if (persistent)
        {
            VkCommandBufferAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            allocInfo.level = secondary ? VK_COMMAND_BUFFER_LEVEL_SECONDARY : VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            allocInfo.commandBufferCount = bufferCount;

            allocInfo.commandPool = GetPersistentPool(workloadType);
            FreeQueuedBuffers(s_ThreadPools.PersistentPools.get());

            if (!FL_VK_CHECK_RESULT(vkAllocateCommandBuffers(
                Context::Devices().Device(),
                &allocInfo,
                buffers
            ), "Allocate persistent command buffers"))
                throw std::exception();
        }
        else
        {
            CreateFramePoolsForThread();
            FreeFrameBuffers(s_ThreadPools.FramePools[Flourish::Context::FrameIndex()].get());
            s_ThreadPools.FramePools[Flourish::Context::FrameIndex()]->GetBuffers(workloadType, buffers, bufferCount);
        }

        return { std::this_thread::get_id(), persistent ? s_ThreadPools.PersistentPools.get() : nullptr, workloadType };
    }

    void Commands::FreeBuffers(const CommandBufferAllocInfo& allocInfo, const VkCommandBuffer* buffers, u32 bufferCount)
    {
        // Frame allocated buffers do not need to be explicitly freed
        if (!allocInfo.PersistentPools)
        {
            FL_LOG_WARN("Freeing command buffer that was not allocated as persistent. This is unnecessary.");
            return;
        }

        // We can free directly if we are allocating and freeing on the same thread
        if (std::this_thread::get_id() == allocInfo.Thread)
        {
            vkFreeCommandBuffers(
                Context::Devices().Device(),
                allocInfo.PersistentPools->Pools.GetPool(allocInfo.WorkloadType),
                bufferCount,
                buffers
            );
        }
        // We can also free directly if the allocated pool is not currently in use. We can do this because we know the pool
        // is not allowed to be written to once a thread is no longer claiming it
        else if (!allocInfo.PersistentPools->InUse)
        {
            allocInfo.PersistentPools->Mutex.lock();
            vkFreeCommandBuffers(
                Context::Devices().Device(),
                allocInfo.PersistentPools->Pools.GetPool(allocInfo.WorkloadType),
                bufferCount,
                buffers
            );
            allocInfo.PersistentPools->Mutex.unlock();            
        }
        // Otherwise we can add the buffers to the active persistent pool so that they can be freed by the thread that is currently
        // using it
        else
        {
            allocInfo.PersistentPools->Mutex.lock();
            for (u32 i = 0; i < bufferCount; i++)
                allocInfo.PersistentPools->PushBufferToFree(allocInfo.WorkloadType, buffers[i]);
            allocInfo.PersistentPools->Mutex.unlock();
        }
    }

    void Commands::FreeBuffer(const CommandBufferAllocInfo& allocInfo, VkCommandBuffer buffer)
    {
        FreeBuffers(allocInfo, &buffer, 1);
    }

    void Commands::PopulateCommandPools(CommandPools* pools, bool allowReset)
    {
        auto device = Context::Devices().Device();

        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = Context::Queues().QueueIndex(GPUWorkloadType::Graphics);
        if (allowReset)
            poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

        if (!FL_VK_CHECK_RESULT(vkCreateCommandPool(
            device,
            &poolInfo,
            nullptr,
            &pools->GraphicsPool
        ), "Create command pool"))
            throw std::exception();
        
        poolInfo.queueFamilyIndex = Context::Queues().QueueIndex(GPUWorkloadType::Compute);

        if (!FL_VK_CHECK_RESULT(vkCreateCommandPool(
            device,
            &poolInfo,
            nullptr,
            &pools->ComputePool
        ), "Create command pool"))
            throw std::exception();

        poolInfo.queueFamilyIndex = Context::Queues().QueueIndex(GPUWorkloadType::Transfer);

        if (!FL_VK_CHECK_RESULT(vkCreateCommandPool(
            device,
            &poolInfo,
            nullptr,
            &pools->TransferPool
        ), "Create command pool"))
            throw std::exception();
    }

    void Commands::DestroyPools(CommandPools* pools)
    {
        auto device = Context::Devices().Device();

        vkDestroyCommandPool(device, pools->GraphicsPool, nullptr);
        vkDestroyCommandPool(device, pools->ComputePool, nullptr);
        vkDestroyCommandPool(device, pools->TransferPool, nullptr);
        pools->GraphicsPool = nullptr;
        pools->ComputePool = nullptr;
        pools->TransferPool = nullptr;
    }

    void Commands::FreeQueuedBuffers(PersistentPools* pools)
    {
        pools->Mutex.lock();
        
        if (!pools->GraphicsToFree.empty())
        {
            vkFreeCommandBuffers(
                Context::Devices().Device(),
                pools->Pools.GraphicsPool,
                pools->GraphicsToFree.size(),
                pools->GraphicsToFree.data()
            );
            pools->GraphicsToFree.clear();
        }
        if (!pools->ComputeToFree.empty())
        {
            vkFreeCommandBuffers(
                Context::Devices().Device(),
                pools->Pools.ComputePool,
                pools->ComputeToFree.size(),
                pools->ComputeToFree.data()
            );
            pools->ComputeToFree.clear();
        }
        if (!pools->TransferToFree.empty())
        {
            vkFreeCommandBuffers(
                Context::Devices().Device(),
                pools->Pools.TransferPool,
                pools->TransferToFree.size(),
                pools->TransferToFree.data()
            );
            pools->TransferToFree.clear();
        }

        pools->Mutex.unlock();
    }

    void Commands::FreeFrameBuffers(FramePools* pools)
    {
        auto device = Context::Devices().Device();

        if (pools->LastAllocationFrame != Flourish::Context::FrameCount())
        {
            pools->LastAllocationFrame = Flourish::Context::FrameCount();
            pools->FreeGraphicsPtr = 0;
            pools->FreeComputePtr = 0;
            pools->FreeTransferPtr = 0;
            FL_VK_ENSURE_RESULT(vkResetCommandPool(device, pools->Pools.GraphicsPool, 0), "Reset command pool");
            FL_VK_ENSURE_RESULT(vkResetCommandPool(device, pools->Pools.ComputePool, 0), "Reset command pool");
            FL_VK_ENSURE_RESULT(vkResetCommandPool(device, pools->Pools.TransferPool, 0), "Reset command pool");
        }
    }
}
