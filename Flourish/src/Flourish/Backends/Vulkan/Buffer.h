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

    private:
        u32 m_BufferCount;
        std::array<VkBuffer, Flourish::Context::MaxFrameBufferCount> m_Buffers;
        std::array<VkBuffer, Flourish::Context::MaxFrameBufferCount> m_StagingBuffers;
        std::array<VmaAllocation, Flourish::Context::MaxFrameBufferCount> m_BufferAllocations;
        std::array<VmaAllocation, Flourish::Context::MaxFrameBufferCount> m_StagingBufferAllocations;
    };
}