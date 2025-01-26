#include "flpch.h"
#include "Commands.h"

#include "Flourish/Backends/Vulkan/Context.h"

namespace Flourish::Vulkan
{
    void PersistentPools::PushBufferToFree(GPUWorkloadType workloadType, VkCommandBuffer buffer)
    {
        BuffersToFree[(u32)workloadType].emplace_back(buffer);
    }

    void FramePools::GetBuffers(GPUWorkloadType workloadType, bool secondary, VkCommandBuffer* buffers, u32 bufferCount)
    {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = secondary ? VK_COMMAND_BUFFER_LEVEL_SECONDARY : VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = bufferCount;

        auto& freeList = secondary ? SecondaryFreeList[(u32)workloadType] : PrimaryFreeList[(u32)workloadType];
        if (freeList.FreePtr + bufferCount > freeList.Free.size())
        {
            freeList.Free.resize(freeList.FreePtr + bufferCount);

            allocInfo.commandPool = Pools[(u32)workloadType];
            if (!FL_VK_CHECK_RESULT(vkAllocateCommandBuffers(
                Context::Devices().Device(),
                &allocInfo, freeList.Free.data() + freeList.FreePtr
            ), "Allocate frame command buffers"))
                throw std::exception();
        }

        memcpy(buffers, freeList.Free.data() + freeList.FreePtr, bufferCount * sizeof(VkCommandBuffer));
        
        freeList.FreePtr += bufferCount;
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
        return s_ThreadPools.PersistentPools->Pools[(u32)workloadType];
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
            s_ThreadPools.FramePools[Flourish::Context::FrameIndex()]->GetBuffers(workloadType, secondary, buffers, bufferCount);
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
                allocInfo.PersistentPools->Pools[(u32)allocInfo.WorkloadType],
                bufferCount,
                buffers
            );
            return;
        }

        // We can also free directly if the allocated pool is not currently in use. We can do this because we know the pool
        // is not allowed to be written to once a thread is no longer claiming it
        allocInfo.PersistentPools->Mutex.lock();
        if (!allocInfo.PersistentPools->InUse)
        {
            vkFreeCommandBuffers(
                Context::Devices().Device(),
                allocInfo.PersistentPools->Pools[(u32)allocInfo.WorkloadType],
                bufferCount,
                buffers
            );
        }
        // Otherwise we can add the buffers to the active persistent pool so that they can be freed by the thread that is currently
        // using it
        else
        {
            for (u32 i = 0; i < bufferCount; i++)
                allocInfo.PersistentPools->PushBufferToFree(allocInfo.WorkloadType, buffers[i]);
        }
        allocInfo.PersistentPools->Mutex.unlock();
    }

    void Commands::FreeBuffer(const CommandBufferAllocInfo& allocInfo, VkCommandBuffer buffer)
    {
        FreeBuffers(allocInfo, &buffer, 1);
    }

    void Commands::SubmitSingleTimeCommands(
        GPUWorkloadType workloadType,
        bool async,
        std::function<void(VkCommandBuffer)>&& recordFn)
    {
        VkCommandBuffer cmdBuf;
        auto allocInfo = Context::Commands().AllocateBuffers(
            workloadType, false, &cmdBuf, 1, true
        );   
        
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        beginInfo.pInheritanceInfo = nullptr;

        FL_VK_ENSURE_RESULT(vkBeginCommandBuffer(cmdBuf, &beginInfo), "SubmitSingleTimeCommands command buffer begin");
        
        recordFn(cmdBuf);

        FL_VK_ENSURE_RESULT(vkEndCommandBuffer(cmdBuf), "SubmitSingleTimeCommands command buffer end");

        if (async)
        {
            Context::Queues().PushCommand(workloadType, cmdBuf, [cmdBuf, allocInfo]()
            {
                Context::Commands().FreeBuffer(allocInfo, cmdBuf);
            });
        }
        else
        {
            Context::Queues().ExecuteCommand(workloadType, cmdBuf, "SubmitSingleTimeCommands execute");
            Context::Commands().FreeBuffer(allocInfo, cmdBuf);
        }
    }

    void Commands::PopulateCommandPools(CommandPools* pools, bool allowReset)
    {
        auto device = Context::Devices().Device();

        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        if (allowReset)
            poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

        for (u32 i = 0; i < pools->size(); i++)
        {
            poolInfo.queueFamilyIndex = Context::Queues().QueueIndex((Flourish::GPUWorkloadType)i);

            if (!FL_VK_CHECK_RESULT(vkCreateCommandPool(
                device,
                &poolInfo,
                nullptr,
                &(*pools)[i]
            ), "Create command pool"))
                throw std::exception();
        }
    }

    void Commands::DestroyPools(CommandPools* pools)
    {
        auto device = Context::Devices().Device();

        for (auto& pool : *pools)
        {
            vkDestroyCommandPool(device, pool, nullptr);
            pool = VK_NULL_HANDLE;
        }
    }

    void Commands::FreeQueuedBuffers(PersistentPools* pools)
    {
        pools->Mutex.lock();
        
        for (u32 i = 0; i < pools->BuffersToFree.size(); i++)
        {
            auto& toFree = pools->BuffersToFree[i];
            if (toFree.empty())
                continue;

            vkFreeCommandBuffers(
                Context::Devices().Device(),
                pools->Pools[i],
                toFree.size(),
                toFree.data()
            );

            toFree.clear();
        }

        pools->Mutex.unlock();
    }

    void Commands::FreeFrameBuffers(FramePools* pools)
    {
        auto device = Context::Devices().Device();

        if (pools->LastAllocationFrame != Flourish::Context::FrameCount())
        {
            pools->LastAllocationFrame = Flourish::Context::FrameCount();
            for (auto& list : pools->PrimaryFreeList)
                list.FreePtr = 0;
            for (auto& list : pools->SecondaryFreeList)
                list.FreePtr = 0;
            for (auto pool : pools->Pools)
                FL_VK_ENSURE_RESULT(vkResetCommandPool(device, pool, 0), "Reset command pool");
        }
    }
}
