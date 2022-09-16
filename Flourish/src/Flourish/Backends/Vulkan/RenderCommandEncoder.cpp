#include "flpch.h"
#include "RenderCommandEncoder.h"

#include "Flourish/Backends/Vulkan/Util/Context.h"
#include "Flourish/Backends/Vulkan/Framebuffer.h"
#include "Flourish/Backends/Vulkan/Buffer.h"

namespace Flourish::Vulkan
{
    RenderCommandEncoder::RenderCommandEncoder(const RenderCommandEncoderCreateInfo& createInfo)
        : Flourish::RenderCommandEncoder(createInfo), m_CommandBuffer(CommandBufferCreateInfo())
    {

    }

    RenderCommandEncoder::~RenderCommandEncoder()
    {

    }

    void RenderCommandEncoder::BeginEncoding()
    {
        FL_CRASH_ASSERT(!m_CommandBuffer.IsRecording(), "Cannot begin render command encoding that has already begun");
        
        m_CommandBuffer.BeginRecording();

        VkCommandBuffer buffer = m_CommandBuffer.GetCommandBuffer();
        Framebuffer* fbuffer = static_cast<Framebuffer*>(m_Info.Framebuffer.get());

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (f32)fbuffer->GetWidth();
        viewport.height = (f32)fbuffer->GetHeight();
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(buffer, 0, 1, &viewport);
        
        VkRect2D scissor{};
        scissor.offset = { 0, 0 };
        scissor.extent = { fbuffer->GetWidth(), fbuffer->GetHeight() };
        vkCmdSetScissor(buffer, 0, 1, &scissor);

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = fbuffer->GetRenderPass();
        renderPassInfo.framebuffer = fbuffer->GetFramebuffer();
        renderPassInfo.renderArea.offset = { 0, 0 };
        renderPassInfo.renderArea.extent = { fbuffer->GetWidth(), fbuffer->GetHeight() };

        renderPassInfo.clearValueCount = static_cast<u32>(fbuffer->GetClearValues().size());
        renderPassInfo.pClearValues = fbuffer->GetClearValues().data();

        vkCmdBeginRenderPass(buffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    }

    void RenderCommandEncoder::EndEncoding()
    {
        FL_CRASH_ASSERT(m_Encoding, "Cannot end render command encoding that has not begun");
        
        vkCmdEndRenderPass(m_CommandBuffer.GetCommandBuffer());
        m_CommandBuffer.EndRecording();
    }

    void RenderCommandEncoder::SetViewport(u32 x, u32 y, u32 width, u32 height)
    {
        FL_CRASH_ASSERT(m_Encoding, "Cannot encode SetViewport before render encoding has begun");

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (f32)width;
        viewport.height = (f32)width;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        vkCmdSetViewport(m_CommandBuffer.GetCommandBuffer(), 0, 1, &viewport);
    }

    void RenderCommandEncoder::BindVertexBuffer(Flourish::Buffer* _buffer)
    {
        FL_CRASH_ASSERT(m_Encoding, "Cannot encode BindVertexBuffer before render encoding has begun");

        VkBuffer buffer = static_cast<Buffer*>(_buffer)->GetBuffer();

        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(m_CommandBuffer.GetCommandBuffer(), 0, 1, &buffer, offsets);
    }

    void RenderCommandEncoder::BindIndexBuffer(Flourish::Buffer* _buffer)
    {
        FL_CRASH_ASSERT(m_Encoding, "Cannot encode BindIndexBuffer before render encoding has begun");

        VkBuffer buffer = static_cast<Buffer*>(_buffer)->GetBuffer();
        
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindIndexBuffer(m_CommandBuffer.GetCommandBuffer(), buffer, 0, VK_INDEX_TYPE_UINT32);
    }

    void RenderCommandEncoder::DrawIndexed(u32 indexCount, u32 indexOffset, u32 vertexOffset, u32 instanceCount)
    {
        FL_CRASH_ASSERT(m_Encoding, "Cannot encode DrawIndexed before render encoding has begun");

        vkCmdDrawIndexed(m_CommandBuffer.GetCommandBuffer(), indexCount, instanceCount, indexOffset, vertexOffset, 0);
    }

    void RenderCommandEncoder::Draw(u32 vertexCount, u32 vertexOffset, u32 instanceCount)
    {

    }
}