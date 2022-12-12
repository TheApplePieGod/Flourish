#include "flpch.h"
#include "Buffer.h"

#include "Flourish/Backends/Vulkan/Util/Context.h"

namespace Flourish::Vulkan
{
    Buffer::Buffer(const BufferCreateInfo& createInfo)
        : Flourish::Buffer(createInfo)
    {
        // Dynamic buffers need a separate buffer for each frame buffer
        if (m_Info.Usage == BufferUsageType::Dynamic)
            m_BufferCount = Flourish::Context::FrameBufferCount();

        #if defined(FL_DEBUG) && defined(FL_LOGGING) 
        if (GetAllocatedSize() == 0)
        { FL_LOG_WARN("Creating a buffer with zero allocated size"); }
        if (m_Info.Usage == BufferUsageType::Static && !m_Info.InitialData)
        { FL_LOG_WARN("Creating a static buffer with no initial data"); }
        if (m_Info.Usage == BufferUsageType::Static && GetAllocatedSize() != m_Info.InitialDataSize)
        { FL_LOG_WARN("Creating a static buffer with initial data of a different size"); }
        #endif

        auto device = Context::Devices().Device();
        VkDeviceSize bufSize = static_cast<u64>(GetAllocatedSize());

        VkBufferCreateInfo bufCreateInfo{};
        bufCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufCreateInfo.size = bufSize;
        bufCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        switch (m_Info.Type)
        {
            default: { FL_CRASH_ASSERT(false, "Failed to create VulkanBuffer of unsupported type") } break;
            case BufferType::Uniform:
            {
                if (m_Info.ElementCount > 1)
                {
                    VkDeviceSize minAlignment = Context::Devices().PhysicalDeviceProperties().limits.minUniformBufferOffsetAlignment;
                    FL_ASSERT(
                        m_Info.Layout.GetStride() % minAlignment == 0,
                        "Uniform buffer layout must be a multiple of %d but is %d",
                        minAlignment,
                        m_Info.Layout.GetStride()
                    );
                }

                bufCreateInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
                m_MemoryDirection = MemoryDirection::CPUToGPU;
            } break;

            case BufferType::Storage:
            {
                if (m_Info.ElementCount > 1)
                {
                    VkDeviceSize minAlignment = Context::Devices().PhysicalDeviceProperties().limits.minStorageBufferOffsetAlignment;
                    FL_ASSERT(
                        m_Info.Layout.GetStride() % minAlignment == 0,
                        "Storage buffer layout must be a multiple of %d but is %d",
                        minAlignment,
                        m_Info.Layout.GetStride()
                    );
                }

                bufCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
                m_MemoryDirection = MemoryDirection::CPUToGPU;
            } break;

            case BufferType::Pixel:
            {
                bufCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
                m_MemoryDirection = MemoryDirection::GPUToCPU;
            } break;

            case BufferType::Indirect:
            {
                bufCreateInfo.usage = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
                m_MemoryDirection = MemoryDirection::CPUToGPU;
            } break;

            case BufferType::Vertex:
            {
                bufCreateInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
                m_MemoryDirection = MemoryDirection::CPUToGPU;
            } break;

            case BufferType::Index:
            {
                bufCreateInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
                m_MemoryDirection = MemoryDirection::CPUToGPU;
            } break;
        }
        
        #if defined(FL_DEBUG) && defined(FL_LOGGING) 
        if (m_MemoryDirection == MemoryDirection::GPUToCPU && m_Info.InitialData)
        { FL_LOG_WARN("Creating a GPU source buffer that was passed initial data but should not have"); }
        #endif

        CreateBuffers(bufCreateInfo);
    }

    Buffer::~Buffer()
    {
        auto buffers = m_Buffers;
        auto stagingBuffers = m_StagingBuffers;
        auto bufferCount = m_BufferCount;
        Context::FinalizerQueue().Push([=]()
        {
            for (u32 i = 0; i < bufferCount; i++)
            {
                if (buffers[i].HasComplement)
                    vmaDestroyBuffer(Context::Allocator(), stagingBuffers[i].Buffer, stagingBuffers[i].Allocation);
                vmaDestroyBuffer(Context::Allocator(), buffers[i].Buffer, buffers[i].Allocation);
            }
        }, "Buffer free");
    }

    void Buffer::SetBytes(void* data, u32 byteCount, u32 byteOffset)
    {
        FL_ASSERT(m_Info.Usage != BufferUsageType::Static, "Attempting to update buffer that is marked as static");
        FL_ASSERT(byteCount + byteOffset <= GetAllocatedSize(), "Attempting to write buffer data that exceeds buffer size");

        auto& bufferData = GetBufferData();
        if (bufferData.HasComplement)
            memcpy((char*)GetStagingBufferData().AllocationInfo.pMappedData + byteOffset, data, byteCount);
        else
            memcpy((char*)bufferData.AllocationInfo.pMappedData + byteOffset, data, byteCount);
    }

    void Buffer::Flush()
    {
        // We don't need to transfer if there is no staging buffer
        auto& bufferData = GetBufferData();
        if (!bufferData.HasComplement) return;

        // Always transfer from staging -> regular
        CopyBufferToBuffer(GetStagingBuffer(), GetBuffer(), GetAllocatedSize());
    }

    VkBuffer Buffer::GetBuffer() const
    {
        return m_BufferCount == 1 ? m_Buffers[0].Buffer : m_Buffers[Flourish::Context::FrameIndex()].Buffer;
    }

    VkBuffer Buffer::GetStagingBuffer() const
    {
        return m_BufferCount == 1 ? m_StagingBuffers[0].Buffer : m_StagingBuffers[Flourish::Context::FrameIndex()].Buffer;
    }

    void Buffer::CopyBufferToBuffer(VkBuffer src, VkBuffer dst, u64 size, VkCommandBuffer buffer)
    {
        // Create and start command buffer if it wasn't passed in
        VkCommandBuffer cmdBuffer = buffer;
        CommandBufferAllocInfo allocInfo;
        if (!buffer)
        {
            allocInfo = Context::Commands().AllocateBuffers(GPUWorkloadType::Transfer, false, &cmdBuffer, 1, true);

            VkCommandBufferBeginInfo beginInfo{};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            beginInfo.pInheritanceInfo = nullptr;

            FL_VK_ENSURE_RESULT(vkBeginCommandBuffer(cmdBuffer, &beginInfo));
        }

        VkBufferCopy copy{};
        copy.srcOffset = 0;
        copy.dstOffset = 0;
        copy.size = size;

        vkCmdCopyBuffer(cmdBuffer, src, dst, 1, &copy);

        if (!buffer)
        {
            FL_VK_ENSURE_RESULT(vkEndCommandBuffer(cmdBuffer));

            Context::Queues().PushCommand(GPUWorkloadType::Transfer, cmdBuffer, [cmdBuffer, allocInfo]()
            {
                Context::Commands().FreeBuffer(allocInfo, cmdBuffer);
            }, "CopyBufferToBuffer command free");
        }
    }

    void Buffer::CopyBufferToImage(VkBuffer src, VkImage dst, u32 imageWidth, u32 imageHeight, VkImageLayout imageLayout, VkCommandBuffer buffer)
    {
        // Create and start command buffer if it wasn't passed in
        VkCommandBuffer cmdBuffer = buffer;
        CommandBufferAllocInfo allocInfo;
        if (!buffer)
        {
            allocInfo = Context::Commands().AllocateBuffers(GPUWorkloadType::Transfer, false, &cmdBuffer, 1, true);

            VkCommandBufferBeginInfo beginInfo{};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            beginInfo.pInheritanceInfo = nullptr;

            FL_VK_ENSURE_RESULT(vkBeginCommandBuffer(cmdBuffer, &beginInfo));
        }

        VkBufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = { 0, 0, 0 };
        region.imageExtent = { imageWidth, imageHeight, 1 };

        vkCmdCopyBufferToImage(cmdBuffer, src, dst, imageLayout, 1, &region);

        if (!buffer)
        {
            FL_VK_ENSURE_RESULT(vkEndCommandBuffer(cmdBuffer));
            
            Context::Queues().PushCommand(GPUWorkloadType::Transfer, cmdBuffer, [cmdBuffer, allocInfo]()
            {
                Context::Commands().FreeBuffer(allocInfo, cmdBuffer);
            }, "CopyBufferToImage command free");
        }
    }

    void Buffer::AllocateStagingBuffer(VkBuffer& buffer, VmaAllocation& alloc, VmaAllocationInfo& allocInfo, u64 size)
    {
        VkBufferCreateInfo bufCreateInfo{};
        bufCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufCreateInfo.size = size;
        bufCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        VmaAllocationCreateInfo allocCreateInfo = {};
        allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
        allocCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                                VMA_ALLOCATION_CREATE_MAPPED_BIT;

        FL_VK_ENSURE_RESULT(vmaCreateBuffer(
            Context::Allocator(),
            &bufCreateInfo,
            &allocCreateInfo,
            &buffer,
            &alloc,
            &allocInfo
        ));
    }

    const Buffer::BufferData& Buffer::GetBufferData() const
    {
        return m_BufferCount == 1 ? m_Buffers[0] : m_Buffers[Flourish::Context::FrameIndex()];
    }

    const Buffer::BufferData& Buffer::GetStagingBufferData() const
    {
        // Buffer count will never be 1 with persistent staging buffers
        return m_StagingBuffers[Flourish::Context::FrameIndex()];
    }

    void Buffer::CreateBuffers(VkBufferCreateInfo bufCreateInfo)
    {
        // Default allocation is device local
        // TODO: dedicated allocation for large allocations
        VmaAllocationCreateInfo allocCreateInfo{};
        allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

        // Assumes that we want cpu visible memory if we have a dyanmic cpu -> gpu buffer
        // TODO: add granularity for this
        bool dynamicHostBuffer = m_MemoryDirection == MemoryDirection::CPUToGPU && m_Info.Usage == BufferUsageType::Dynamic;
        if (dynamicHostBuffer)
        {
            allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
            allocCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                                    VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT |
                                    VMA_ALLOCATION_CREATE_MAPPED_BIT;
        }
        
        // Allocate buffers and determine & allocate any staging buffers
        // TODO: handle edge case when some parts of the buffer may be staging and others are dynamic host
        for (u32 i = 0; i < m_BufferCount; i++)
        {
            FL_VK_ENSURE_RESULT(vmaCreateBuffer(
                Context::Allocator(),
                &bufCreateInfo,
                &allocCreateInfo,
                &m_Buffers[i].Buffer,
                &m_Buffers[i].Allocation,
                &m_Buffers[i].AllocationInfo
            ));

            if (dynamicHostBuffer)
            {
                VkMemoryPropertyFlags memPropFlags;
                vmaGetAllocationMemoryProperties(Context::Allocator(), m_Buffers[i].Allocation, &memPropFlags);
                if (!(memPropFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT))
                    m_Buffers[i].HasComplement = true;
            }
            else if (m_MemoryDirection == MemoryDirection::GPUToCPU)
                // All gpu->cpu buffers will have a complement
                m_Buffers[i].HasComplement = true;

            if (m_Buffers[i].HasComplement)
            {
                AllocateStagingBuffer(
                    m_StagingBuffers[i].Buffer,
                    m_StagingBuffers[i].Allocation,
                    m_StagingBuffers[i].AllocationInfo,
                    bufCreateInfo.size
                );
            }
        }
        
        // Initial data transfers only apply when buffer is cpu->gpu
        BufferData initialDataStagingBuf;
        if (m_MemoryDirection == MemoryDirection::CPUToGPU && m_Info.InitialData && m_Info.InitialDataSize > 0)
        {
            for (u32 i = 0; i < m_BufferCount; i++)
            {
                // Static buffers always need to have initial data transferred into them and any cpu->gpu
                // dynamic buffers will also need to have data transferred
                if (m_Info.Usage == BufferUsageType::Static || m_Buffers[i].HasComplement)
                {
                    if (!initialDataStagingBuf.Buffer)
                    {
                        AllocateStagingBuffer(
                            initialDataStagingBuf.Buffer,
                            initialDataStagingBuf.Allocation,
                            initialDataStagingBuf.AllocationInfo,
                            bufCreateInfo.size
                        );

                        memcpy(initialDataStagingBuf.AllocationInfo.pMappedData, m_Info.InitialData, m_Info.InitialDataSize);
                    }
                    
                    CopyBufferToBuffer(initialDataStagingBuf.Buffer, m_Buffers[i].Buffer, m_Info.InitialDataSize);
                }
                else
                    // Otherwise this is a dynamic host buffer that we can directly write to
                    memcpy(m_Buffers[i].AllocationInfo.pMappedData, m_Info.InitialData, m_Info.InitialDataSize);
            }
        }
        
        // Destroy the temp staging buffer if it was created
        if (initialDataStagingBuf.Buffer)
        {
            Context::FinalizerQueue().Push([initialDataStagingBuf]()
            {
                vmaDestroyBuffer(Context::Allocator(), initialDataStagingBuf.Buffer, initialDataStagingBuf.Allocation);
            }, "Buffer free staging");
        }
    }
}