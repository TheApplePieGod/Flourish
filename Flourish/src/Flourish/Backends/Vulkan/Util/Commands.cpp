#include "flpch.h"
#include "Commands.h"

#include "Flourish/Backends/Vulkan/Util/Context.h"

namespace Flourish::Vulkan
{
    void Commands::Initialize()
    {

    }

    void Commands::Shutdown()
    {
        auto device = Context::Devices().Device();

        for (auto& pair : m_ThreadCommandPools)
            DestroyPools(pair.second);
        for (auto& pools : m_UnusedPools)
            DestroyPools(pools);
    }

    VkCommandPool Commands::GetPool(GPUWorkloadType workloadType, std::thread::id thread)
    {
        FL_ASSERT(Flourish::Context::IsThreadRegistered(), "Attempting to perform operations on a non-registered thread");

        m_ThreadCommandPoolsLock.lock();
        auto pools = m_ThreadCommandPools[thread];
        m_ThreadCommandPoolsLock.unlock();
        switch (workloadType)
        {
            case GPUWorkloadType::Graphics:
            { return pools.GraphicsPool; }
            case GPUWorkloadType::Transfer:
            { return pools.TransferPool; }
            case GPUWorkloadType::Compute:
            { return pools.ComputePool; }
        }

        FL_ASSERT(false, "Command pool for workload not supported");
        return nullptr;
    }

    void Commands::CreatePoolsForThread()
    {
        std::thread::id thread = std::this_thread::get_id();
        auto device = Context::Devices().Device();

        ThreadCommandPools pools;
        if (m_UnusedPools.size() > 0)
        {
            m_UnusedPoolsLock.lock();
            pools = m_UnusedPools.back();
            m_UnusedPools.pop_back();
            m_UnusedPoolsLock.unlock();
        }
        else
        {
            VkCommandPoolCreateInfo poolInfo{};
            poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            poolInfo.queueFamilyIndex = Context::Queues().GraphicsQueueIndex();
            poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; // allow resetting

            FL_VK_ENSURE_RESULT(vkCreateCommandPool(device, &poolInfo, nullptr, &pools.GraphicsPool));
            
            poolInfo.queueFamilyIndex = Context::Queues().ComputeQueueIndex();

            FL_VK_ENSURE_RESULT(vkCreateCommandPool(device, &poolInfo, nullptr, &pools.ComputePool));

            poolInfo.queueFamilyIndex = Context::Queues().TransferQueueIndex();

            FL_VK_ENSURE_RESULT(vkCreateCommandPool(device, &poolInfo, nullptr, &pools.TransferPool));
        }

        m_ThreadCommandPoolsLock.lock();
        m_ThreadCommandPools.emplace(thread, pools);
        m_ThreadCommandPoolsLock.unlock();
    }

    void Commands::DestroyPoolsForThread()
    {
        std::thread::id thread = std::this_thread::get_id();
        m_ThreadCommandPoolsLock.lock();
        auto pools = m_ThreadCommandPools[thread];
        m_ThreadCommandPools.erase(thread);
        m_ThreadCommandPoolsLock.unlock();

        m_UnusedPoolsLock.lock();
        m_UnusedPools.push_back(pools);
        m_UnusedPoolsLock.unlock();
    }

    void Commands::AllocateBuffers(GPUWorkloadType workloadType, bool secondary, VkCommandBuffer* buffers, u32 bufferCount, std::thread::id thread)
    {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = secondary ? VK_COMMAND_BUFFER_LEVEL_SECONDARY : VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = bufferCount;
        allocInfo.commandPool = GetPool(workloadType, thread);

        // Not optimal to lock all pools for every thread here, but it removes the need for complex
        // overhead and tracking, and it should be relatively negligible due to the low frequency
        // of buffer allocations (especially parallel)
        m_ThreadCommandPoolsLock.lock();
        FL_VK_ENSURE_RESULT(vkAllocateCommandBuffers(Context::Devices().Device(), &allocInfo, buffers));
        m_ThreadCommandPoolsLock.unlock();
    }

    void Commands::FreeBuffers(GPUWorkloadType workloadType, const VkCommandBuffer* buffers, u32 bufferCount, std::thread::id thread)
    {
        auto pool = GetPool(workloadType, thread);
        Context::DeleteQueue().Push([this, pool, buffers, bufferCount]()
        {
            m_ThreadCommandPoolsLock.lock();
            vkFreeCommandBuffers(
                Context::Devices().Device(),
                pool,
                bufferCount,
                buffers
            );
            m_ThreadCommandPoolsLock.unlock();
        });
    }

    void Commands::DestroyPools(const ThreadCommandPools& pools)
    {
        auto device = Context::Devices().Device();

        vkDestroyCommandPool(device, pools.GraphicsPool, nullptr);
        vkDestroyCommandPool(device, pools.ComputePool, nullptr);
        vkDestroyCommandPool(device, pools.TransferPool, nullptr);
    }
}