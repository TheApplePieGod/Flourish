#pragma once

#include "Flourish/Api/Buffer.h"
#include "Flourish/Backends/Vulkan/Util/Common.h"

namespace Flourish::Vulkan
{
    class Buffer : public Flourish::Buffer
    {
    public:
        enum class MemoryDirection
        {
            CPUToGPU = 0,
            GPUToCPU
        };

    public:
        Buffer(const BufferCreateInfo& createInfo);
        Buffer(
            const BufferCreateInfo& createInfo,
            VkBufferUsageFlags usageFlags,
            MemoryDirection memoryDirection,
            VkCommandBuffer uploadBuffer = VK_NULL_HANDLE,
            bool forceDeviceMemory = false
        );
        ~Buffer() override;

        void SetBytes(const void* data, u32 byteCount, u32 byteOffset) override;
        void ReadBytes(void* outData, u32 byteCount, u32 byteOffset) const override;
        void Flush(bool immediate) override;
        void* GetBufferGPUAddress() const override;

        void FlushInternal(VkCommandBuffer buffer, bool execute = false);

        // TS
        VkBuffer GetBuffer() const;
        VkBuffer GetBuffer(u32 frameIndex) const;
        VkBuffer GetStagingBuffer() const;
        VkBuffer GetStagingBuffer(u32 frameIndex) const;

    public:
        static void CopyBufferToBuffer(
            VkBuffer src,
            VkBuffer dst,
            u32 srcOffset,
            u32 dstOffset,
            u32 size,
            VkCommandBuffer buffer = VK_NULL_HANDLE,
            bool execute = false,
            std::function<void()> callback = nullptr
        );
        static void CopyBufferToImage(
            VkBuffer src,
            VkImage dst,
            VkImageAspectFlags dstAspect,
            u32 imageWidth,
            u32 imageHeight,
            u32 dstMipLevel,
            u32 dstLayerIndex,
            VkImageLayout imageLayout,
            VkCommandBuffer buffer = VK_NULL_HANDLE
        );
        static void CopyImageToBuffer(
            VkImage src,
            VkImageAspectFlags srcAspect,
            VkBuffer dst,
            u32 imageWidth,
            u32 imageHeight,
            u32 srcMipLevel,
            u32 srcLayerIndex,
            VkImageLayout imageLayout,
            VkCommandBuffer buffer = VK_NULL_HANDLE
        );
        static void AllocateStagingBuffer(VkBuffer& buffer, VmaAllocation& alloc, VmaAllocationInfo& allocInfo, u64 size);

    private:
        static void ImageBufferCopyInternal(
            VkImage image,
            VkImageAspectFlags aspect,
            VkBuffer buffer,
            u32 imageWidth,
            u32 imageHeight,
            u32 mipLevel,
            u32 layerIndex,
            VkImageLayout imageLayout,
            bool imageSrc,
            VkCommandBuffer cmdBuf = VK_NULL_HANDLE
        );

    private:
        struct BufferData
        {
            VkBuffer Buffer = VK_NULL_HANDLE;
            VkDeviceAddress DeviceAddress = 0;
            VmaAllocation Allocation;
            VmaAllocationInfo AllocationInfo;
            bool HasComplement = false;
        };

    private:
        void CreateInternal(
            VkBufferUsageFlags usage,
            MemoryDirection memDirection,
            VkCommandBuffer uploadBuffer,
            bool forceDeviceMemory
        );
        const BufferData& GetBufferData() const;
        const BufferData& GetStagingBufferData() const;
        void CreateBuffers(
            VkBufferCreateInfo bufCreateInfo,
            VkCommandBuffer uploadBuffer,
            bool forceDeviceMemory
        );

    private:
        u32 m_BufferCount = 1;
        MemoryDirection m_MemoryDirection;
        std::array<BufferData, Flourish::Context::MaxFrameBufferCount> m_Buffers;
        std::array<BufferData, Flourish::Context::MaxFrameBufferCount> m_StagingBuffers;
    };
}
