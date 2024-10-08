#include "flpch.h"
#include "Buffer.h"

#include "Flourish/Backends/Vulkan/Context.h"
#include "Flourish/Backends/Vulkan/TransferCommandEncoder.h"

namespace Flourish::Vulkan
{
    Buffer::Buffer(const BufferCreateInfo& createInfo)
        : Flourish::Buffer(createInfo)
    {
        #if defined(FL_DEBUG) && defined(FL_LOGGING) 
        if (m_Info.Usage == BufferUsageType::Static && !m_Info.InitialData)
            FL_LOG_WARN("Creating a static buffer with no initial data");
        if (m_Info.Usage == BufferUsageType::Static && GetAllocatedSize() != m_Info.InitialDataSize)
            FL_LOG_WARN("Creating a static buffer with initial data of a different size");
        if (m_Info.Stride != 0 && m_Info.Stride % 4 != 0)
            FL_LOG_WARN("Buffer has explicit stride %d that is not four byte aligned", m_Info.Stride);
        #endif

        VkBufferUsageFlags usage = 0;
        MemoryDirection memDirection;
        switch (m_Info.Type)
        {
            default: { FL_CRASH_ASSERT(false, "Failed to create VulkanBuffer of unsupported type") } break;
            case BufferType::Uniform:
            {
                usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
                memDirection = MemoryDirection::CPUToGPU;
            } break;

            case BufferType::Storage:
            {
                usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
                memDirection = MemoryDirection::CPUToGPU;
            } break;

            case BufferType::Pixel:
            {
                memDirection = MemoryDirection::GPUToCPU;
            } break;

            case BufferType::Indirect:
            {
                usage = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
                memDirection = MemoryDirection::CPUToGPU;
            } break;

            case BufferType::Vertex:
            {
                usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
                memDirection = MemoryDirection::CPUToGPU;
            } break;

            case BufferType::Index:
            {
                usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
                memDirection = MemoryDirection::CPUToGPU;
            } break;
        }

        // TODO: this probably isn't great but we have no good way of specifying this in the api
        usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

        if (m_Info.CanCreateAccelerationStructure)
        {
            FL_ASSERT(
                Flourish::Context::FeatureTable().RayTracing,
                "RayTracing feature must be enabled to create a buffer with acceleration structure support"
            );

            usage |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
            m_Info.ExposeGPUAddress = true;
        }

        VkCommandBuffer uploadBuffer = nullptr;
        if (m_Info.UploadEncoder)
        {
            FL_ASSERT(
                m_Info.UploadEncoder->IsEncoding(),
                "Cannot create buffer with UploadEncoder that is not encoding"
            );

            auto encoder = static_cast<TransferCommandEncoder*>(m_Info.UploadEncoder);
            encoder->MarkManuallyRecorded();
            uploadBuffer = encoder->GetCommandBuffer();
        }
        
        CreateInternal(usage, memDirection, uploadBuffer, false);
    }

    Buffer::Buffer(
        const BufferCreateInfo& createInfo,
        VkBufferUsageFlags usageFlags,
        MemoryDirection memoryDirection,
        VkCommandBuffer uploadBuffer,
        bool forceDeviceMemory
    ) : Flourish::Buffer(createInfo)
    {
        CreateInternal(usageFlags, memoryDirection, uploadBuffer, forceDeviceMemory);
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
                if (buffers[i].HasComplement && stagingBuffers[i].Buffer)
                    vmaDestroyBuffer(Context::Allocator(), stagingBuffers[i].Buffer, stagingBuffers[i].Allocation);
                if (buffers[i].Buffer)
                    vmaDestroyBuffer(Context::Allocator(), buffers[i].Buffer, buffers[i].Allocation);
            }
        }, "Buffer free");
    }

    void Buffer::CreateInternal(VkBufferUsageFlags usage, MemoryDirection memDirection, VkCommandBuffer uploadBuffer, bool forceDeviceMemory)
    {
        m_MemoryDirection = memDirection;

        // Dynamic buffers need a separate buffer for each frame buffer
        if (m_Info.Usage == BufferUsageType::Dynamic)
            m_BufferCount = Flourish::Context::FrameBufferCount();

        if (GetAllocatedSize() == 0)
        {
            FL_LOG_ERROR("Cannot create a buffer with zero size");
            throw std::exception();
        }

        auto device = Context::Devices().Device();
        VkDeviceSize bufSize = static_cast<u64>(GetAllocatedSize());

        VkBufferCreateInfo bufCreateInfo{};
        bufCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufCreateInfo.size = bufSize;
        bufCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        bufCreateInfo.usage = usage;

        if (m_Info.ExposeGPUAddress)
        {
            FL_ASSERT(
                Flourish::Context::FeatureTable().BufferGPUAddress,
                "BufferGPUAddress feature must be enabled to create a buffer with ExposeGPUAddress"
            );
            bufCreateInfo.usage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
        }

        #if defined(FL_DEBUG) && defined(FL_LOGGING) 
        if (m_MemoryDirection == MemoryDirection::GPUToCPU && m_Info.InitialData)
            FL_LOG_WARN("Creating a GPU source buffer that was passed initial data but should not have");
        #endif

        CreateBuffers(bufCreateInfo, uploadBuffer, forceDeviceMemory);
    }

    void Buffer::SetBytes(const void* data, u32 byteCount, u32 byteOffset)
    {
        FL_ASSERT(m_Info.Usage != BufferUsageType::Static, "Attempting to update buffer that is marked as static");
        FL_CRASH_ASSERT(byteCount + byteOffset <= GetAllocatedSize(), "Attempting to write buffer data that exceeds buffer size");

        auto& bufferData = GetBufferData();
        if (bufferData.HasComplement)
            memcpy((char*)GetStagingBufferData().AllocationInfo.pMappedData + byteOffset, data, byteCount);
        else
            memcpy((char*)bufferData.AllocationInfo.pMappedData + byteOffset, data, byteCount);
    }

    void Buffer::ReadBytes(void* outData, u32 byteCount, u32 byteOffset) const
    {
        FL_CRASH_ASSERT(byteCount + byteOffset <= GetAllocatedSize(), "Attempting to read buffer data that exceeds buffer size");

        auto& bufferData = GetBufferData();
        if (bufferData.HasComplement)
            memcpy(outData, (char*)GetStagingBufferData().AllocationInfo.pMappedData + byteOffset, byteCount);
        else
            memcpy(outData, (char*)bufferData.AllocationInfo.pMappedData + byteOffset, byteCount);
    }

    void Buffer::Flush(bool immediate)
    {
        FlushInternal(nullptr, immediate);
    }

    void Buffer::FlushInternal(VkCommandBuffer buffer, bool execute)
    {
        // We don't need to transfer if there is no staging buffer
        auto& bufferData = GetBufferData();
        if (!bufferData.HasComplement) return;

        // GPU -> CPU will transfer Regular -> Staging while CPU -> GPU will do the reverse
        if (m_MemoryDirection == MemoryDirection::GPUToCPU)
            CopyBufferToBuffer(GetBuffer(), GetStagingBuffer(), 0, 0, GetAllocatedSize(), buffer, execute);
        else
            CopyBufferToBuffer(GetStagingBuffer(), GetBuffer(), 0, 0, GetAllocatedSize(), buffer, execute);
    }

    VkBuffer Buffer::GetBuffer() const
    {
        return GetBuffer(Flourish::Context::FrameIndex());
    }

    VkBuffer Buffer::GetBuffer(u32 frameIndex) const
    {
        if (m_BufferCount == 1) return m_Buffers[0].Buffer;
        return m_Buffers[frameIndex].Buffer;
    }

    VkBuffer Buffer::GetStagingBuffer() const
    {
        return GetStagingBuffer(Flourish::Context::FrameIndex());
    }

    VkBuffer Buffer::GetStagingBuffer(u32 frameIndex) const
    {
        if (m_BufferCount == 1) return m_StagingBuffers[0].Buffer;
        return m_StagingBuffers[frameIndex].Buffer;
    }

    void* Buffer::GetBufferGPUAddress() const
    {
        FL_ASSERT(m_Info.ExposeGPUAddress, "Buffer must be created with ExposeGPUAddress to query buffer address");

        if (m_BufferCount == 1) return (void*)m_Buffers[0].DeviceAddress;
        return (void*)m_Buffers[Flourish::Context::FrameIndex()].DeviceAddress;
    }

    void Buffer::CopyBufferToBuffer(
        VkBuffer src,
        VkBuffer dst,
        u32 srcOffset,
        u32 dstOffset,
        u32 size,
        VkCommandBuffer buffer,
        bool execute,
        std::function<void()> callback
    )
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

            FL_VK_ENSURE_RESULT(vkBeginCommandBuffer(cmdBuffer, &beginInfo), "CopyBufferToBuffer command buffer begin");
        }

        VkBufferCopy copy{};
        copy.srcOffset = srcOffset;
        copy.dstOffset = dstOffset;
        copy.size = size;

        vkCmdCopyBuffer(cmdBuffer, src, dst, 1, &copy);

        if (!buffer)
        {
            FL_VK_ENSURE_RESULT(vkEndCommandBuffer(cmdBuffer), "CopyBufferToBuffer command buffer end");

            if (execute)
            {
                Context::Queues().ExecuteCommand(GPUWorkloadType::Transfer, cmdBuffer, "CopyBufferToBuffer execute");
                Context::Commands().FreeBuffer(allocInfo, cmdBuffer);
            }
            else
            {
                Context::Queues().PushCommand(GPUWorkloadType::Transfer, cmdBuffer, [cmdBuffer, allocInfo, callback]()
                {
                    Context::Commands().FreeBuffer(allocInfo, cmdBuffer);
                    if (callback)
                        callback();
                }); //, "CopyBufferToBuffer command free");
            }
        }
    }

    void Buffer::CopyBufferToImage(
        VkBuffer src,
        VkImage dst,
        VkImageAspectFlags dstAspect,
        u32 bufferOffset,
        u32 imageWidth,
        u32 imageHeight,
        u32 dstMipLevel,
        u32 dstLayerIndex,
        VkImageLayout imageLayout,
        VkCommandBuffer buffer)
    {
        ImageBufferCopyInternal(dst, dstAspect, src, bufferOffset, imageWidth, imageHeight, dstMipLevel, dstLayerIndex, imageLayout, false, buffer);
    }

    void Buffer::CopyImageToBuffer(
        VkImage src,
        VkImageAspectFlags srcAspect,
        VkBuffer dst,
        u32 bufferOffset,
        u32 imageWidth,
        u32 imageHeight,
        u32 srcMipLevel,
        u32 srcLayerIndex,
        VkImageLayout imageLayout,
        VkCommandBuffer buffer)
    {
        ImageBufferCopyInternal(src, srcAspect, dst, bufferOffset, imageWidth, imageHeight, srcMipLevel, srcLayerIndex, imageLayout, true, buffer);
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

        if (!FL_VK_CHECK_RESULT(vmaCreateBuffer(
            Context::Allocator(),
            &bufCreateInfo,
            &allocCreateInfo,
            &buffer,
            &alloc,
            &allocInfo
        ), "Buffer create staging buffer"))
            throw std::exception();
    }

    void Buffer::ImageBufferCopyInternal(
        VkImage image,
        VkImageAspectFlags aspect,
        VkBuffer buffer,
        u32 bufferOffset,
        u32 imageWidth,
        u32 imageHeight,
        u32 mipLevel,
        u32 layerIndex,
        VkImageLayout imageLayout,
        bool imageSrc,
        VkCommandBuffer cmdBuf)
    {
        // Create and start command buffer if it wasn't passed in
        VkCommandBuffer cmdBuffer = cmdBuf;
        CommandBufferAllocInfo allocInfo;
        if (!cmdBuf)
        {
            allocInfo = Context::Commands().AllocateBuffers(GPUWorkloadType::Transfer, false, &cmdBuffer, 1, true);

            VkCommandBufferBeginInfo beginInfo{};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            beginInfo.pInheritanceInfo = nullptr;

            FL_VK_ENSURE_RESULT(vkBeginCommandBuffer(cmdBuffer, &beginInfo), "ImageBufferCopy command buffer begin");
        }

        VkBufferImageCopy region{};
        region.bufferOffset = bufferOffset;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = aspect;
        region.imageSubresource.mipLevel = mipLevel;
        region.imageSubresource.baseArrayLayer = layerIndex;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = { 0, 0, 0 };
        region.imageExtent = { imageWidth, imageHeight, 1 };

        if (imageSrc)
            vkCmdCopyImageToBuffer(cmdBuffer, image, imageLayout, buffer, 1, &region);
        else
            vkCmdCopyBufferToImage(cmdBuffer, buffer, image, imageLayout, 1, &region);

        if (!cmdBuf)
        {
            FL_VK_ENSURE_RESULT(vkEndCommandBuffer(cmdBuffer), "ImageBufferCopy command buffer end");
            
            Context::Queues().PushCommand(GPUWorkloadType::Transfer, cmdBuffer, [cmdBuffer, allocInfo]()
            {
                Context::Commands().FreeBuffer(allocInfo, cmdBuffer);
            }, "ImageBufferCopyInternal command free");
        }
    }

    const Buffer::BufferData& Buffer::GetBufferData() const
    {
        return m_BufferCount == 1 ? m_Buffers[0] : m_Buffers[Flourish::Context::FrameIndex()];
    }

    const Buffer::BufferData& Buffer::GetStagingBufferData() const
    {
        return m_BufferCount == 1 ? m_StagingBuffers[0] : m_StagingBuffers[Flourish::Context::FrameIndex()];
    }

    void Buffer::CreateBuffers(
        VkBufferCreateInfo bufCreateInfo,
        VkCommandBuffer uploadBuffer,
        bool forceDeviceMemory
    )
    {
        // Default allocation is device local
        // TODO: dedicated allocation for large allocations
        VmaAllocationCreateInfo allocCreateInfo{};
        allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

        // Assumes that we want cpu visible memory if we have a dyanmic cpu -> gpu buffer
        // TODO: add granularity for this
        bool dynamicHostBuffer = m_MemoryDirection == MemoryDirection::CPUToGPU && (m_Info.Usage == BufferUsageType::Dynamic || m_Info.Usage == BufferUsageType::DynamicOneFrame);
        if (dynamicHostBuffer && !forceDeviceMemory)
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
            if (!FL_VK_CHECK_RESULT(vmaCreateBuffer(
                Context::Allocator(),
                &bufCreateInfo,
                &allocCreateInfo,
                &m_Buffers[i].Buffer,
                &m_Buffers[i].Allocation,
                &m_Buffers[i].AllocationInfo
            ), "Buffer create buffer"))
                throw std::exception();

            if (m_Info.ExposeGPUAddress)
            {
                VkBufferDeviceAddressInfo addInfo{};
                addInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
                addInfo.buffer = m_Buffers[i].Buffer;
                m_Buffers[i].DeviceAddress = vkGetBufferDeviceAddressKHR(Context::Devices().Device(), &addInfo);
            }

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
        bool gpuCopy = false;
        BufferData initialDataStagingBuf;
        if (m_MemoryDirection == MemoryDirection::CPUToGPU && m_Info.InitialData && m_Info.InitialDataSize > 0)
        {
            for (u32 i = 0; i < m_BufferCount; i++)
            {
                // Static buffers always need to have initial data transferred into them and any cpu->gpu
                // dynamic buffers will also need to have data transferred
                if (m_Info.Usage == BufferUsageType::Static || m_Buffers[i].HasComplement)
                {
                    VkBuffer srcBuffer;
                    if (m_Buffers[i].HasComplement)
                    {
                        srcBuffer = m_StagingBuffers[i].Buffer;
                        memcpy(m_StagingBuffers[i].AllocationInfo.pMappedData, m_Info.InitialData, m_Info.InitialDataSize);
                    }
                    else
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
                        srcBuffer = initialDataStagingBuf.Buffer;
                    }
                    
                    CopyBufferToBuffer(
                        srcBuffer,
                        m_Buffers[i].Buffer,
                        0, 0,
                        m_Info.InitialDataSize,
                        uploadBuffer,
                        true,
                        nullptr
                    );
                    gpuCopy = true;
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
