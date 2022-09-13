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
        if (m_Info.Usage == BufferUsageType::Static && m_Info.InitialData == nullptr)
        { FL_LOG_WARN("Creating a static buffer with no initial data"); }
        if (m_Info.Usage == BufferUsageType::Static && GetAllocatedSize() != m_Info.InitialDataSize)
        { FL_LOG_WARN("Creating a static buffer with initial data of a different size"); }
        #endif

        auto device = Context::Devices().Device();
        VkDeviceSize bufSize = static_cast<u64>(GetAllocatedSize());

        VkBufferCreateInfo bufCreateInfo{};
        bufCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufCreateInfo.size = bufSize;
        VmaAllocationCreateInfo allocCreateInfo{};
        switch (m_Info.Type)
        {
            default: { FL_CRASH_ASSERT(false, "Failed to create VulkanBuffer of unsupported type") } break;
            case BufferType::Uniform:
            {
                if (m_Info.ElementCount > 1)
                {
                    VkDeviceSize minAlignment = Context::Devices().PhysicalDeviceProperties().limits.minUniformBufferOffsetAlignment;
                    FL_ASSERT(
                        m_Info.Layout.GetStride() % minAlignment != 0,
                        "Uniform buffer layout must be a multiple of {0} but is {1}",
                        minAlignment,
                        m_Info.Layout.GetStride()
                    );
                }

                bufCreateInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
                allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
                allocCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                                        VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT |
                                        VMA_ALLOCATION_CREATE_MAPPED_BIT;
                
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

                    VkMemoryPropertyFlags memPropFlags;
                    vmaGetAllocationMemoryProperties(Context::Allocator(), m_Buffers[i].Allocation, &memPropFlags);
                    if (memPropFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
                        memcpy(m_Buffers[i].AllocationInfo.pMappedData, m_Info.InitialData, m_Info.InitialDataSize);
                    else
                    {
                        m_Buffers[i].HasComplement = true;
                        AllocateStagingBuffer(
                            m_StagingBuffers[i].Buffer,
                            m_StagingBuffers[i].Allocation,
                            m_StagingBuffers[i].AllocationInfo,
                            bufSize
                        );

                        if (m_Info.InitialData && m_Info.InitialDataSize > 0)
                        {
                            memcpy(m_StagingBuffers[i].AllocationInfo.pMappedData, m_Info.InitialData, m_Info.InitialDataSize);
                            CopyBufferToBuffer(m_StagingBuffers[i].Buffer, m_Buffers[i].Buffer, m_Info.InitialDataSize);
                        }
                    }
                }
            } break;
        }
    }

    Buffer::~Buffer()
    {
        auto buffers = m_Buffers;
        auto stagingBuffers = m_StagingBuffers;
        auto bufferCount = m_BufferCount;
        Context::DeleteQueue().Push([=]()
        {
            for (u32 i = 0; i < bufferCount; i++)
            {
                if (buffers[i].HasComplement)
                    vmaDestroyBuffer(Context::Allocator(), stagingBuffers[i].Buffer, stagingBuffers[i].Allocation);
                vmaDestroyBuffer(Context::Allocator(), buffers[i].Buffer, buffers[i].Allocation);
            }
        });
    }

    void Buffer::SetBytes(void* data, u32 byteCount, u32 byteOffset)
    {
        FL_ASSERT(m_Info.Usage != BufferUsageType::Static, "Attempting to update buffer that is marked as static");

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
        return m_BufferCount == 1 ? m_Buffers[0].Buffer : m_Buffers[Context::FrameIndex()].Buffer;
    }

    VkBuffer Buffer::GetStagingBuffer() const
    {
        return m_BufferCount == 1 ? m_StagingBuffers[0].Buffer : m_StagingBuffers[Context::FrameIndex()].Buffer;
    }

    void Buffer::CopyBufferToBuffer(VkBuffer src, VkBuffer dst, u64 size, VkCommandBuffer buffer)
    {
        // Create and start command buffer if it wasn't passed in
        VkCommandBuffer cmdBuffer = buffer;
        if (!buffer)
        {
            Context::Commands().AllocateBuffers(GPUWorkloadType::Transfer, false, &cmdBuffer, 1);

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

            auto thread = std::this_thread::get_id();
            Context::Queues().PushCommand(GPUWorkloadType::Transfer, cmdBuffer, [cmdBuffer, thread](){
                Context::Commands().FreeBuffers(GPUWorkloadType::Transfer, &cmdBuffer, 1, thread);
            });
        }
    }

    void Buffer::CopyBufferToImage(VkBuffer src, VkImage dst, u32 imageWidth, u32 imageHeight, VkImageLayout imageLayout, VkCommandBuffer buffer)
    {
        // Create and start command buffer if it wasn't passed in
        VkCommandBuffer cmdBuffer = buffer;
        if (!buffer)
        {
            Context::Commands().AllocateBuffers(GPUWorkloadType::Transfer, false, &cmdBuffer, 1);

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
            
            auto thread = std::this_thread::get_id();
            Context::Queues().PushCommand(GPUWorkloadType::Transfer, cmdBuffer, [cmdBuffer, thread](){
                Context::Commands().FreeBuffers(GPUWorkloadType::Transfer, &cmdBuffer, 1, thread);
            });
        }
    }

    void Buffer::AllocateStagingBuffer(VkBuffer& buffer, VmaAllocation& alloc, VmaAllocationInfo& allocInfo, u64 size)
    {
        VkBufferCreateInfo bufCreateInfo{};
        bufCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufCreateInfo.size = size;
        bufCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
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
        return m_BufferCount == 1 ? m_Buffers[0] : m_Buffers[Context::FrameIndex()];
    }

    const Buffer::BufferData& Buffer::GetStagingBufferData() const
    {
        // Buffer count will never be 1 with persistent staging buffers
        return m_StagingBuffers[Context::FrameIndex()];
    }
}