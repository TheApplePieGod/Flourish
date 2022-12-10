#include "flpch.h"
#include "Commands.h"

#include "Flourish/Backends/Vulkan/Util/Context.h"

namespace Flourish::Vulkan
{
    VkCommandPool ThreadCommandPoolsData::GetPool(GPUWorkloadType workloadType)
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

    void ThreadCommandPoolsData::PushBufferToFree(GPUWorkloadType workloadType, VkCommandBuffer buffer)
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

    ThreadCommandPools::ThreadCommandPools()
    {
        // Ensure this does not run before vulkan is initialized
        if (!Context::Devices().Device()) return;
        Context::Commands().CreatePoolsForThread();
    }

    ThreadCommandPools::~ThreadCommandPools()
    {
        if (!Context::Devices().Device()) return;
        Context::Commands().DestroyPoolsForThread();
    }

    void Commands::Initialize()
    {
        // Ensure pools for the main thread have been initialized
        if (!s_ThreadPools.Data) CreatePoolsForThread();
    }

    void Commands::Shutdown()
    {
        auto device = Context::Devices().Device();

        for (auto& pair : m_PoolsInUse)
            DestroyPools(pair.second.get());
        for (auto& pools : m_UnusedPools)
            DestroyPools(pools.get());
        
        m_PoolsInUse.clear();
        m_UnusedPools.clear();
    }

    VkCommandPool Commands::GetPool(GPUWorkloadType workloadType)
    {
        return s_ThreadPools.Data->GetPool(workloadType);
    }

    void Commands::CreatePoolsForThread()
    {
        auto device = Context::Devices().Device();

        if (m_UnusedPools.size() > 0)
        {
            m_PoolsLock.lock();
            s_ThreadPools.Data = m_UnusedPools.back();
            m_UnusedPools.pop_back();
            m_PoolsLock.unlock();
        }
        else
        {
            s_ThreadPools.Data = std::make_shared<ThreadCommandPoolsData>();

            VkCommandPoolCreateInfo poolInfo{};
            poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            poolInfo.queueFamilyIndex = Context::Queues().QueueIndex(GPUWorkloadType::Graphics);
            poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; // allow resetting

            FL_VK_ENSURE_RESULT(vkCreateCommandPool(device, &poolInfo, nullptr, &s_ThreadPools.Data->GraphicsPool));
            
            poolInfo.queueFamilyIndex = Context::Queues().QueueIndex(GPUWorkloadType::Compute);

            FL_VK_ENSURE_RESULT(vkCreateCommandPool(device, &poolInfo, nullptr, &s_ThreadPools.Data->ComputePool));

            poolInfo.queueFamilyIndex = Context::Queues().QueueIndex(GPUWorkloadType::Transfer);

            FL_VK_ENSURE_RESULT(vkCreateCommandPool(device, &poolInfo, nullptr, &s_ThreadPools.Data->TransferPool));
        }

        m_PoolsLock.lock();
        m_PoolsInUse[std::this_thread::get_id()] = s_ThreadPools.Data;
        m_PoolsLock.unlock();
    }

    void Commands::DestroyPoolsForThread()
    {
        m_PoolsLock.lock();
        if (s_ThreadPools.Data)
            m_UnusedPools.push_back(s_ThreadPools.Data);
        m_PoolsInUse.erase(std::this_thread::get_id());
        m_PoolsLock.unlock();
    }

    CommandBufferAllocInfo Commands::AllocateBuffers(GPUWorkloadType workloadType, bool secondary, VkCommandBuffer* buffers, u32 bufferCount)
    {
        FreeQueuedBuffers(s_ThreadPools.Data.get());

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = secondary ? VK_COMMAND_BUFFER_LEVEL_SECONDARY : VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = bufferCount;
        allocInfo.commandPool = GetPool(workloadType);

        FL_VK_ENSURE_RESULT(vkAllocateCommandBuffers(Context::Devices().Device(), &allocInfo, buffers));
        
        return { s_ThreadPools.Data.get(), std::this_thread::get_id(), workloadType };
    }

    void Commands::FreeBuffers(const CommandBufferAllocInfo& allocInfo, const std::vector<VkCommandBuffer>& buffers)
    {
        // We can free directly if we are allocating and freeing on the same thread
        if (std::this_thread::get_id() == allocInfo.Thread)
        {
            vkFreeCommandBuffers(
                Context::Devices().Device(),
                GetPool(allocInfo.WorkloadType),
                buffers.size(),
                buffers.data()
            );
        }
        else
        {
            allocInfo.Pools->Mutex.lock();

            // Add the buffers to be freed to the free queue. We don't want to free them directly because although
            // we may be able to keep track of whether or not the pool is being used by a thread, we can't trust that
            // there aren't lingering buffers being written to
            for (auto buffer : buffers)
                allocInfo.Pools->PushBufferToFree(allocInfo.WorkloadType, buffer);

            allocInfo.Pools->Mutex.unlock();
        }
    }

    void Commands::FreeBuffer(const CommandBufferAllocInfo& allocInfo, VkCommandBuffer buffer)
    {
        if (std::this_thread::get_id() == allocInfo.Thread)
        {
            vkFreeCommandBuffers(
                Context::Devices().Device(),
                GetPool(allocInfo.WorkloadType),
                1, &buffer
            );
        }
        else
        {
            allocInfo.Pools->Mutex.lock();
            allocInfo.Pools->PushBufferToFree(allocInfo.WorkloadType, buffer);
            allocInfo.Pools->Mutex.unlock();
        }
    }

    void Commands::DestroyPools(ThreadCommandPoolsData* pools)
    {
        auto device = Context::Devices().Device();

        vkDestroyCommandPool(device, pools->GraphicsPool, nullptr);
        vkDestroyCommandPool(device, pools->ComputePool, nullptr);
        vkDestroyCommandPool(device, pools->TransferPool, nullptr);
        pools->GraphicsPool = nullptr;
        pools->ComputePool = nullptr;
        pools->TransferPool = nullptr;
    }

    void Commands::FreeQueuedBuffers(ThreadCommandPoolsData* pools)
    {
        pools->Mutex.lock();
        
        if (!pools->GraphicsToFree.empty())
        {
            vkFreeCommandBuffers(
                Context::Devices().Device(),
                pools->GraphicsPool,
                pools->GraphicsToFree.size(),
                pools->GraphicsToFree.data()
            );
            pools->GraphicsToFree.clear();
        }
        if (!pools->ComputeToFree.empty())
        {
            vkFreeCommandBuffers(
                Context::Devices().Device(),
                pools->ComputePool,
                pools->ComputeToFree.size(),
                pools->ComputeToFree.data()
            );
            pools->ComputeToFree.clear();
        }
        if (!pools->TransferToFree.empty())
        {
            vkFreeCommandBuffers(
                Context::Devices().Device(),
                pools->TransferPool,
                pools->TransferToFree.size(),
                pools->TransferToFree.data()
            );
            pools->TransferToFree.clear();
        }

        pools->Mutex.unlock();
    }
}