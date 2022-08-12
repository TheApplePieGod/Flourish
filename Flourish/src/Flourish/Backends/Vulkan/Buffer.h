#pragma once

#include "Flourish/Api/Buffer.h"
#include "Flourish/Backends/Vulkan/Util/Common.h"

namespace Flourish::Vulkan
{
    class Buffer : public Flourish::Buffer
    {
    public:
        Buffer(const BufferCreateInfo& createInfo);
        ~Buffer() override;

        void Flush() override;

        // TS
        VkBuffer GetBuffer() const;
        VkBuffer GetStagingBuffer() const;

    private:
        struct BufferData
        {
            VkBuffer Buffer;
            VmaAllocation Allocation;
            VmaAllocationInfo AllocationInfo;
            bool HasComplement = false;
        };

    private:
        void FlushInternal(VkBuffer src, VkBuffer dst, u64 size);
        void AllocateStagingBuffer(VkBuffer& buffer, VmaAllocation& alloc, VmaAllocationInfo allocInfo, u64 size);

        const BufferData& GetBufferData() const;

    private:
        u32 m_BufferCount = 1;
        std::array<BufferData, Flourish::Context::MaxFrameBufferCount> m_Buffers;
        std::array<BufferData, Flourish::Context::MaxFrameBufferCount> m_StagingBuffers;
    };
}