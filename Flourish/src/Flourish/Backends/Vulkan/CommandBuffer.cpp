#include "flpch.h"
#include "CommandBuffer.h"

#include "Flourish/Backends/Vulkan/Util/Context.h"
#include "Flourish/Api/RenderCommandEncoder.h"
#include "Flourish/Backends/Vulkan/RenderCommandEncoder.h"
#include "Flourish/Backends/Vulkan/Framebuffer.h"

namespace Flourish::Vulkan
{
    CommandBuffer::CommandBuffer(const CommandBufferCreateInfo& createInfo, bool isPrimary)
        : Flourish::CommandBuffer(createInfo)
    {
        m_AllocatedThread = std::this_thread::get_id();
    }

    CommandBuffer::~CommandBuffer()
    {
        FL_ASSERT(
            m_AllocatedThread == std::this_thread::get_id(),
            "Command buffer should never be destroyed from a thread different than the one that created it"
        );
        FL_ASSERT(
            Flourish::Context::IsThreadRegistered(),
            "Destroying command buffer on deregistered thread, which can cause a memory leak"
        );
    }
    
    void CommandBuffer::SubmitEncodedCommands(VkCommandBuffer buffer, GPUWorkloadType workloadType)
    {
        m_EncoderSubmissions.emplace_back(buffer, workloadType);
        m_Encoding = false;
    }

    Flourish::RenderCommandEncoder* CommandBuffer::EncodeRenderCommands(Flourish::Framebuffer* framebuffer)
    {
        FL_CRASH_ASSERT(!m_Encoding, "Cannot begin encoding while another encoding is in progress");
        m_Encoding = true;

        if (m_RenderCommandEncoderCachePtr >= m_RenderCommandEncoderCache.size())
            m_RenderCommandEncoderCache.emplace_back(this);

        m_RenderCommandEncoderCache[m_RenderCommandEncoderCachePtr].BeginEncoding(
            static_cast<Framebuffer*>(framebuffer)
        );

        return static_cast<Flourish::RenderCommandEncoder*>(&m_RenderCommandEncoderCache[m_RenderCommandEncoderCachePtr++]);
    }
    
    VkCommandBuffer CommandBuffer::GetCommandBuffer() const
    {
        return m_CommandBuffers[Flourish::Context::FrameIndex()];
    }
}