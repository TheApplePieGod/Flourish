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
            VkCommandBuffer uploadBuffer = nullptr,
            bool forceDeviceMemory = false
        );
        ~Buffer() override;

        void SetBytes(const void* data, u32 byteCount, u32 byteOffset) override;
        void ReadBytes(void* outData, u32 byteCount, u32 byteOffset) const override;
        void Flush(bool immediate) override;

        void FlushInternal(VkCommandBuffer buffer, bool execute = false);

        // TS
        VkBuffer GetBuffer() const;
        VkBuffer GetBuffer(u32 frameIndex) const;
        VkBuffer GetStagingBuffer() const;
        VkBuffer GetStagingBuffer(u32 frameIndex) const;
        VkDeviceAddress GetBufferDeviceAddress() const;
        VkDeviceAddress GetBufferDeviceAddress(u32 frameIndex) const;

    public:
        static void CopyBufferToBuffer(
            VkBuffer src,
            VkBuffer dst,
            u64 size,
            VkCommandBuffer buffer = nullptr,
            bool execute = false,
            std::function<void()> callback = nullptr
        );
        static void CopyBufferToImage(
            VkBuffer src,
            VkImage dst,
            u32 imageWidth,
            u32 imageHeight,
            VkImageLayout imageLayout,
            VkCommandBuffer buffer = nullptr
        );
        static void CopyImageToBuffer(
            VkImage src,
            VkBuffer dst,
            u32 imageWidth,
            u32 imageHeight,
            VkImageLayout imageLayout,
            VkCommandBuffer buffer = nullptr
        );
        static void AllocateStagingBuffer(VkBuffer& buffer, VmaAllocation& alloc, VmaAllocationInfo& allocInfo, u64 size);

    private:
        static void ImageBufferCopyInternal(
            VkImage image,
            VkBuffer buffer,
            u32 imageWidth,
            u32 imageHeight,
            VkImageLayout imageLayout,
            bool imageSrc,
            VkCommandBuffer cmdBuf = nullptr
        );

    private:
        struct BufferData
        {
            VkBuffer Buffer = nullptr;
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
        bool m_UseDeviceAddress = false;
        MemoryDirection m_MemoryDirection;
        std::array<BufferData, Flourish::Context::MaxFrameBufferCount> m_Buffers;
        std::array<BufferData, Flourish::Context::MaxFrameBufferCount> m_StagingBuffers;
    };
}
