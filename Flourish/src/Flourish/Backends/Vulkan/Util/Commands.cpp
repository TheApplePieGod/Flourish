#include "flpch.h"
#include "Commands.h"

#include "Flourish/Backends/Vulkan/Util/Context.h"

namespace Flourish::Vulkan
{
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
        if (!s_ThreadPools.Data.GraphicsPool) CreatePoolsForThread();
    }

    void Commands::Shutdown()
    {
        auto device = Context::Devices().Device();

        for (auto& pair : m_PoolsInUse)
            DestroyPools(pair.second);
        for (auto& pools : m_UnusedPools)
            DestroyPools(pools);
        
        m_PoolsInUse.clear();
        m_UnusedPools.clear();
    }

    VkCommandPool Commands::GetPool(GPUWorkloadType workloadType)
    {
        switch (workloadType)
        {
            case GPUWorkloadType::Graphics:
            { return s_ThreadPools.Data.GraphicsPool; }
            case GPUWorkloadType::Transfer:
            { return s_ThreadPools.Data.TransferPool; }
            case GPUWorkloadType::Compute:
            { return s_ThreadPools.Data.ComputePool; }
        }

        FL_ASSERT(false, "Command pool for workload not supported");
        return nullptr;
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
            VkCommandPoolCreateInfo poolInfo{};
            poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            poolInfo.queueFamilyIndex = Context::Queues().QueueIndex(GPUWorkloadType::Graphics);
            poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; // allow resetting

            FL_VK_ENSURE_RESULT(vkCreateCommandPool(device, &poolInfo, nullptr, &s_ThreadPools.Data.GraphicsPool));
            
            poolInfo.queueFamilyIndex = Context::Queues().QueueIndex(GPUWorkloadType::Compute);

            FL_VK_ENSURE_RESULT(vkCreateCommandPool(device, &poolInfo, nullptr, &s_ThreadPools.Data.ComputePool));

            poolInfo.queueFamilyIndex = Context::Queues().QueueIndex(GPUWorkloadType::Transfer);

            FL_VK_ENSURE_RESULT(vkCreateCommandPool(device, &poolInfo, nullptr, &s_ThreadPools.Data.TransferPool));
        }

        m_PoolsLock.lock();
        m_PoolsInUse[std::this_thread::get_id()] = s_ThreadPools.Data;
        m_PoolsLock.unlock();
    }

    void Commands::DestroyPoolsForThread()
    {
        m_PoolsLock.lock();
        m_UnusedPools.push_back(s_ThreadPools.Data);
        m_PoolsInUse.erase(std::this_thread::get_id());
        m_PoolsLock.unlock();
    }

    void Commands::AllocateBuffers(GPUWorkloadType workloadType, bool secondary, VkCommandBuffer* buffers, u32 bufferCount)
    {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = secondary ? VK_COMMAND_BUFFER_LEVEL_SECONDARY : VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = bufferCount;
        allocInfo.commandPool = GetPool(workloadType);

        FL_VK_ENSURE_RESULT(vkAllocateCommandBuffers(Context::Devices().Device(), &allocInfo, buffers));
    }

    void Commands::FreeBuffers(GPUWorkloadType workloadType, const std::vector<VkCommandBuffer>& buffers)
    {
        vkFreeCommandBuffers(
            Context::Devices().Device(),
            GetPool(workloadType),
            buffers.size(),
            buffers.data()
        );
    }

    void Commands::FreeBuffer(GPUWorkloadType workloadType, VkCommandBuffer buffer)
    {
        vkFreeCommandBuffers(
            Context::Devices().Device(),
            GetPool(workloadType),
            1,
            &buffer
        );
    }

    void Commands::DestroyPools(const ThreadCommandPoolsData& pools)
    {
        auto device = Context::Devices().Device();

        vkDestroyCommandPool(device, pools.GraphicsPool, nullptr);
        vkDestroyCommandPool(device, pools.ComputePool, nullptr);
        vkDestroyCommandPool(device, pools.TransferPool, nullptr);
    }
}