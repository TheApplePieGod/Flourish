#include "flpch.h"
#include "RenderCommandEncoder.h"

#include "Flourish/Backends/Vulkan/Util/Context.h"
#include "Flourish/Backends/Vulkan/Framebuffer.h"
#include "Flourish/Backends/Vulkan/Buffer.h"
#include "Flourish/Backends/Vulkan/RenderPass.h"

namespace Flourish::Vulkan
{
    RenderCommandEncoder::RenderCommandEncoder(const RenderCommandEncoderCreateInfo& createInfo)
        : Flourish::RenderCommandEncoder(createInfo),
          m_CommandBuffer({ GPUWorkloadType::Graphics, false }, false)
    {
    }

    RenderCommandEncoder::~RenderCommandEncoder()
    {

    }

    void RenderCommandEncoder::BeginEncoding()
    {
        FL_CRASH_ASSERT(!m_CommandBuffer.IsRecording(), "Cannot begin render command encoding that has already begun");
        
        Framebuffer* fbuffer = static_cast<Framebuffer*>(m_Info.Framebuffer.get());
        VkCommandBufferInheritanceInfo inheritanceInfo{};
        inheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
        inheritanceInfo.framebuffer = fbuffer->GetFramebuffer();
        inheritanceInfo.renderPass = fbuffer->GetRenderPass();
        m_CommandBuffer.BeginRecording();

        SetViewport(0, 0, fbuffer->GetWidth(), fbuffer->GetHeight());
        SetScissor(0, 0, fbuffer->GetWidth(), fbuffer->GetHeight());
    }

    void RenderCommandEncoder::EndEncoding()
    {
        FL_CRASH_ASSERT(m_CommandBuffer.IsRecording(), "Cannot end render command encoding that has not begun");
        
        m_CommandBuffer.EndRecording();
    }

    void RenderCommandEncoder::SetViewport(u32 x, u32 y, u32 width, u32 height)
    {
        FL_CRASH_ASSERT(m_CommandBuffer.IsRecording(), "Cannot encode SetViewport before render encoding has begun");

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (f32)width;
        viewport.height = (f32)width;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        vkCmdSetViewport(m_CommandBuffer.GetCommandBuffer(), 0, 1, &viewport);
    }

    void RenderCommandEncoder::SetScissor(u32 x, u32 y, u32 width, u32 height)
    {
        FL_CRASH_ASSERT(m_CommandBuffer.IsRecording(), "Cannot encode SetScissor before render encoding has begun");

        VkRect2D scissor{};
        scissor.offset = { 0, 0 };
        scissor.extent = { width, height };

        vkCmdSetScissor(m_CommandBuffer.GetCommandBuffer(), 0, 1, &scissor);
    }

    void RenderCommandEncoder::BindVertexBuffer(Flourish::Buffer* _buffer)
    {
        FL_CRASH_ASSERT(m_CommandBuffer.IsRecording(), "Cannot encode BindVertexBuffer before render encoding has begun");

        VkBuffer buffer = static_cast<Buffer*>(_buffer)->GetBuffer();

        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(m_CommandBuffer.GetCommandBuffer(), 0, 1, &buffer, offsets);
    }

    void RenderCommandEncoder::BindIndexBuffer(Flourish::Buffer* _buffer)
    {
        FL_CRASH_ASSERT(m_CommandBuffer.IsRecording(), "Cannot encode BindIndexBuffer before render encoding has begun");

        VkBuffer buffer = static_cast<Buffer*>(_buffer)->GetBuffer();
        
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindIndexBuffer(m_CommandBuffer.GetCommandBuffer(), buffer, 0, VK_INDEX_TYPE_UINT32);
    }

    void RenderCommandEncoder::DrawIndexed(u32 indexCount, u32 indexOffset, u32 vertexOffset, u32 instanceCount)
    {
        FL_CRASH_ASSERT(m_CommandBuffer.IsRecording(), "Cannot encode DrawIndexed before render encoding has begun");

        vkCmdDrawIndexed(m_CommandBuffer.GetCommandBuffer(), indexCount, instanceCount, indexOffset, vertexOffset, 0);
    }

    void RenderCommandEncoder::Draw(u32 vertexCount, u32 vertexOffset, u32 instanceCount)
    {

    }

    const VkRenderPassBeginInfo& RenderCommandEncoder::GetRenderPassBeginInfo()
    {
        Framebuffer* fbuffer = static_cast<Framebuffer*>(m_Info.Framebuffer.get());

        m_RenderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        m_RenderPassBeginInfo.renderPass = fbuffer->GetRenderPass();
        m_RenderPassBeginInfo.framebuffer = fbuffer->GetFramebuffer();
        m_RenderPassBeginInfo.renderArea.offset = { 0, 0 };
        m_RenderPassBeginInfo.renderArea.extent = { fbuffer->GetWidth(), fbuffer->GetHeight() };
        m_RenderPassBeginInfo.clearValueCount = static_cast<u32>(fbuffer->GetClearValues().size());
        m_RenderPassBeginInfo.pClearValues = fbuffer->GetClearValues().data();
    }
}