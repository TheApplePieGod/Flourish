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
        if (m_Info.Stride != 0 && m_Info.Stride % 4 != 0)
            FL_LOG_WARN("Buffer has explicit stride %d that is not four byte aligned", m_Info.Stride);
        #endif

        VkBufferUsageFlags usage = Common::ConvertBufferUsage(m_Info.Usage);

        // TODO: this probably isn't great but we have no good way of specifying this in the api
        usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

        if (m_Info.Usage & BufferUsageFlags::AccelerationStructureBuild)
        {
            FL_ASSERT(
                Flourish::Context::FeatureTable().RayTracing,
                "RayTracing feature must be enabled to create a buffer with acceleration structure build support"
            );

            // Force expose GPU address
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
        
        CreateInternal(usage, uploadBuffer);
    }

    Buffer::Buffer(
        const BufferCreateInfo& createInfo,
        VkBufferUsageFlags usageFlags,
        VkCommandBuffer uploadBuffer
    ) : Flourish::Buffer(createInfo)
    {
        CreateInternal(usageFlags, uploadBuffer);
    }

    Buffer::~Buffer()
    {
        auto buffers = m_BufferAllocations;
        Context::FinalizerQueue().Push([=]()
        {
            for (u32 i = 0; i < buffers.size(); i++)
                if (buffers[i].Buffer)
                    vmaDestroyBuffer(Context::Allocator(), buffers[i].Buffer, buffers[i].Allocation);
        }, "Buffer free");
    }

    void Buffer::CreateInternal(VkBufferUsageFlags usage, VkCommandBuffer uploadBuffer)
    {
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

        CreateBuffers(bufCreateInfo, uploadBuffer);
    }

    void Buffer::SetBytes(const void* data, u32 byteCount, u32 byteOffset)
    {
        FL_ASSERT(
            m_Info.MemoryType == BufferMemoryType::CPUWrite ||
            m_Info.MemoryType == BufferMemoryType::CPUWriteFrame,
            "Attempting to update buffer that does not have the CPUWrite memory type"
        );
        FL_CRASH_ASSERT(byteCount + byteOffset <= GetAllocatedSize(), "Attempting to write buffer data that exceeds buffer size");

        auto& bufferData = GetWriteBufferData();
        memcpy((char*)bufferData.AllocationInfo.pMappedData + byteOffset, data, byteCount);
    }

    void Buffer::ReadBytes(void* outData, u32 byteCount, u32 byteOffset) const
    {
        FL_ASSERT(m_Info.MemoryType == BufferMemoryType::CPURead, "Attempting to read buffer that does not have the CPURead memory type");
        FL_CRASH_ASSERT(byteCount + byteOffset <= GetAllocatedSize(), "Attempting to read buffer data that exceeds buffer size");

        auto& bufferData = GetFlushBufferData();
        memcpy(outData, (char*)bufferData.AllocationInfo.pMappedData + byteOffset, byteCount);
    }

    void Buffer::Flush(bool immediate)
    {
        FlushInternal(nullptr, immediate);
    }

    void Buffer::FlushInternal(VkCommandBuffer buffer, bool execute)
    {
        auto& write = GetWriteBufferData();
        auto& flush = GetFlushBufferData();

        // We don't need to do anything if flushBuf == writeBuf
        if (write.Buffer == flush.Buffer) return;

        CopyBufferToBuffer(write.Buffer, flush.Buffer, 0, 0, GetAllocatedSize(), buffer, execute);
    }

    const Buffer::BufferData& Buffer::GetGPUBufferData(u32 frameIndex) const
    {
        switch (m_Info.MemoryType)
        {
            case BufferMemoryType::GPUOnly:
            case BufferMemoryType::CPURead:
                return GetWriteBufferData(frameIndex);
            case BufferMemoryType::CPUWrite:
            case BufferMemoryType::CPUWriteFrame:
                return GetFlushBufferData(frameIndex);
        }
    }

    const Buffer::BufferData& Buffer::GetWriteBufferData() const
    {
        return GetWriteBufferData(Flourish::Context::FrameIndex());
    }

    const Buffer::BufferData& Buffer::GetFlushBufferData() const
    {
        return GetFlushBufferData(Flourish::Context::FrameIndex());
    }

    const Buffer::BufferData& Buffer::GetWriteBufferData(u32 frameIndex) const
    {
        if (m_BufferCount == 1) return m_BufferAllocations[m_WriteBuffers[0]];
        return m_BufferAllocations[m_WriteBuffers[frameIndex]];
    }

    const Buffer::BufferData& Buffer::GetFlushBufferData(u32 frameIndex) const
    {
        if (m_BufferCount == 1) return m_BufferAllocations[m_FlushBuffers[0]];
        return m_BufferAllocations[m_FlushBuffers[frameIndex]];
    }

    VkBuffer Buffer::GetGPUBuffer() const
    {
        return GetGPUBuffer(Flourish::Context::FrameIndex());
    }

    VkBuffer Buffer::GetGPUBuffer(u32 frameIndex) const
    {
        return GetGPUBufferData(frameIndex).Buffer;
    }

    VkBuffer Buffer::GetWriteBuffer() const
    {
        return GetWriteBufferData().Buffer;
    }

    VkBuffer Buffer::GetFlushBuffer() const
    {
        return GetFlushBufferData().Buffer;
    }

    void* Buffer::GetBufferGPUAddress() const
    {
        FL_ASSERT(m_Info.ExposeGPUAddress, "Buffer must be created with ExposeGPUAddress to query buffer address");

        return (void*)GetGPUBufferData(Flourish::Context::FrameIndex()).DeviceAddress;
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

    void Buffer::CreateBuffers(
        VkBufferCreateInfo bufCreateInfo,
        VkCommandBuffer uploadBuffer
    )
    {
        // Default allocation is device local
        // TODO: dedicated allocation for large allocations
        VmaAllocationCreateInfo allocCreateInfo{};
        allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

        if (m_Info.MemoryType == BufferMemoryType::CPUWrite || m_Info.MemoryType == BufferMemoryType::CPUWriteFrame)
        {
            allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
            allocCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                                    VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT |
                                    VMA_ALLOCATION_CREATE_MAPPED_BIT;
        }

        const auto AllocateBuffer = [&]()
        {
            u32 allocId = m_BufferAllocations.size();
            BufferData& data = m_BufferAllocations.emplace_back();
            if (!FL_VK_CHECK_RESULT(vmaCreateBuffer(
                Context::Allocator(),
                &bufCreateInfo,
                &allocCreateInfo,
                &data.Buffer,
                &data.Allocation,
                &data.AllocationInfo
            ), "Buffer create buffer"))
                throw std::exception();

            if (m_Info.ExposeGPUAddress)
            {
                VkBufferDeviceAddressInfo addInfo{};
                addInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
                addInfo.buffer = data.Buffer;
                data.DeviceAddress = vkGetBufferDeviceAddressKHR(Context::Devices().Device(), &addInfo);
            }
            return allocId;
        };

        const auto AllocateStaging = [&]()
        {
            u32 allocId = m_BufferAllocations.size();
            BufferData& data = m_BufferAllocations.emplace_back();
            AllocateStagingBuffer(
                data.Buffer,
                data.Allocation,
                data.AllocationInfo,
                bufCreateInfo.size
            );
            return allocId;
        };
        
        switch (m_Info.MemoryType)
        {
            case BufferMemoryType::GPUOnly:
            {
                u32 allocId = AllocateBuffer();
                m_WriteBuffers[0] = allocId;
                m_FlushBuffers[0] = allocId;
            } break;
            case BufferMemoryType::CPURead:
            {
                m_WriteBuffers[0] = AllocateBuffer();
                m_FlushBuffers[0] = AllocateStaging();
            } break;
            case BufferMemoryType::CPUWrite:
            case BufferMemoryType::CPUWriteFrame:
            {
                if (m_Info.MemoryType == BufferMemoryType::CPUWriteFrame)
                    m_BufferCount = Flourish::Context::FrameBufferCount();

                u32 gpuOnlyAllocId = std::numeric_limits<u32>::max();
                for (u32 i = 0; i < m_BufferCount; i++)
                {
                    // Attempt to allocate a host coherent buffer. If it fails, we only
                    // need at most one GPU only buffer, so use that if it was already
                    // allocated

                    u32 allocId;
                    if (gpuOnlyAllocId != std::numeric_limits<u32>::max())
                        allocId = gpuOnlyAllocId;
                    else
                        allocId = AllocateBuffer();

                    BufferData& data = m_BufferAllocations[allocId];

                    VkMemoryPropertyFlags memPropFlags;
                    vmaGetAllocationMemoryProperties(Context::Allocator(), data.Allocation, &memPropFlags);
                    if (!(memPropFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT))
                    {
                        m_WriteBuffers[i] = AllocateStaging();
                        gpuOnlyAllocId = allocId;
                    }
                    else
                        m_WriteBuffers[i] = allocId;
                    m_FlushBuffers[i] = allocId;
                }
            } break;
        }
        
        BufferData initialDataStagingBuf;
        if (m_Info.InitialData && m_Info.InitialDataSize > 0)
        {
            for (u32 i = 0; i < m_BufferCount; i++)
            {
                BufferData srcData;
                VkBuffer dstBuffer = m_BufferAllocations[m_FlushBuffers[i]].Buffer;

                switch (m_Info.MemoryType)
                {
                    case BufferMemoryType::CPUWrite:
                    case BufferMemoryType::CPUWriteFrame:
                    {
                        // CPU writes already have a staging buffer allocated
                        srcData = m_BufferAllocations[m_WriteBuffers[i]];
                    } break;
                    case BufferMemoryType::CPURead:
                    {
                        // CPU read can be written to directly
                        srcData = m_BufferAllocations[m_FlushBuffers[i]];
                    } break;
                    case BufferMemoryType::GPUOnly:
                    {
                        if (!initialDataStagingBuf.Buffer)
                        {
                            AllocateStagingBuffer(
                                initialDataStagingBuf.Buffer,
                                initialDataStagingBuf.Allocation,
                                initialDataStagingBuf.AllocationInfo,
                                bufCreateInfo.size
                            );
                        }
                        srcData = initialDataStagingBuf;
                    } break;
                }

                // TODO: don't need to recopy if buffer was already written to
                memcpy(srcData.AllocationInfo.pMappedData, m_Info.InitialData, m_Info.InitialDataSize);

                if (srcData.Buffer == dstBuffer) continue;

                CopyBufferToBuffer(
                    srcData.Buffer,
                    dstBuffer,
                    0, 0,
                    m_Info.InitialDataSize,
                    uploadBuffer,
                    true,
                    nullptr
                );
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
