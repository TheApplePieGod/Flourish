#include "flpch.h"
#include "RenderCommandEncoder.h"

#include "Flourish/Backends/Vulkan/Util/Context.h"
#include "Flourish/Backends/Vulkan/Framebuffer.h"
#include "Flourish/Backends/Vulkan/Buffer.h"
#include "Flourish/Backends/Vulkan/RenderPass.h"
#include "Flourish/Backends/Vulkan/CommandBuffer.h"

namespace Flourish::Vulkan
{
    RenderCommandEncoder::RenderCommandEncoder(CommandBuffer* parentBuffer)
    {
        m_ParentBuffer = parentBuffer;
        m_AllocatedThread = std::this_thread::get_id();
        Context::Commands().AllocateBuffers(
            GPUWorkloadType::Graphics,
            false,
            m_CommandBuffers.data(),
            Flourish::Context::FrameBufferCount()
        );   
    }

    RenderCommandEncoder::~RenderCommandEncoder()
    {
        // We shouldn't have to do any thread sanity checking here because command buffer
        // already does this and it is the only class who will own this object
        auto buffers = m_CommandBuffers;
        auto thread = m_AllocatedThread;
        Context::DeleteQueue().Push([=]()
        {
            Context::Commands().FreeBuffers(
                GPUWorkloadType::Graphics,
                buffers.data(),
                Flourish::Context::FrameBufferCount(),
                thread
            );
        });
    }

    void RenderCommandEncoder::BeginEncoding(Framebuffer* framebuffer)
    {
        m_Encoding = true;
    
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        // TODO: check result?
        VkCommandBuffer buffer = GetCommandBuffer();
        vkResetCommandBuffer(buffer, 0);
        vkBeginCommandBuffer(buffer, &beginInfo);

        VkRenderPassBeginInfo rpBeginInfo{};
        rpBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rpBeginInfo.renderPass = framebuffer->GetRenderPass();
        rpBeginInfo.framebuffer = framebuffer->GetFramebuffer();
        rpBeginInfo.renderArea.offset = { 0, 0 };
        rpBeginInfo.renderArea.extent = { framebuffer->GetWidth(), framebuffer->GetHeight() };
        rpBeginInfo.clearValueCount = static_cast<u32>(framebuffer->GetClearValues().size());
        rpBeginInfo.pClearValues = framebuffer->GetClearValues().data();
        vkCmdBeginRenderPass(buffer, &rpBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        SetViewport(0, 0, framebuffer->GetWidth(), framebuffer->GetHeight());
        SetScissor(0, 0, framebuffer->GetWidth(), framebuffer->GetHeight());
    }

    void RenderCommandEncoder::EndEncoding()
    {
        FL_CRASH_ASSERT(m_Encoding, "Cannot end encoding that has already ended");
        m_Encoding = false;

        VkCommandBuffer buffer = GetCommandBuffer();
        vkCmdEndRenderPass(buffer);
        vkEndCommandBuffer(buffer);
        m_ParentBuffer->SubmitEncodedCommands(buffer, GPUWorkloadType::Graphics);
    }

    void RenderCommandEncoder::SetViewport(u32 x, u32 y, u32 width, u32 height)
    {
        FL_CRASH_ASSERT(m_Encoding, "Cannot encode SetViewport after encoding has ended");

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (f32)width;
        viewport.height = (f32)width;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        vkCmdSetViewport(GetCommandBuffer(), 0, 1, &viewport);
    }

    void RenderCommandEncoder::SetScissor(u32 x, u32 y, u32 width, u32 height)
    {
        FL_CRASH_ASSERT(m_Encoding, "Cannot encode SetScissor after encoding has ended");

        VkRect2D scissor{};
        scissor.offset = { 0, 0 };
        scissor.extent = { width, height };

        vkCmdSetScissor(GetCommandBuffer(), 0, 1, &scissor);
    }

    void RenderCommandEncoder::BindVertexBuffer(Flourish::Buffer* _buffer)
    {
        FL_CRASH_ASSERT(m_Encoding, "Cannot encode BindVertexBuffer after encoding has ended");

        VkBuffer buffer = static_cast<Buffer*>(_buffer)->GetBuffer();

        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(GetCommandBuffer(), 0, 1, &buffer, offsets);
    }

    void RenderCommandEncoder::BindIndexBuffer(Flourish::Buffer* _buffer)
    {
        FL_CRASH_ASSERT(m_Encoding, "Cannot encode BindIndexBuffer after encoding has ended");

        VkBuffer buffer = static_cast<Buffer*>(_buffer)->GetBuffer();
        
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindIndexBuffer(GetCommandBuffer(), buffer, 0, VK_INDEX_TYPE_UINT32);
    }

    void RenderCommandEncoder::DrawIndexed(u32 indexCount, u32 indexOffset, u32 vertexOffset, u32 instanceCount)
    {
        FL_CRASH_ASSERT(m_Encoding, "Cannot encode DrawIndexed after encoding has ended");

        vkCmdDrawIndexed(GetCommandBuffer(), indexCount, instanceCount, indexOffset, vertexOffset, 0);
    }

    void RenderCommandEncoder::Draw(u32 vertexCount, u32 vertexOffset, u32 instanceCount)
    {
        FL_CRASH_ASSERT(m_Encoding, "Cannot encode Draw after encoding has ended");
    }

    VkCommandBuffer RenderCommandEncoder::GetCommandBuffer() const
    {
        return m_CommandBuffers[Flourish::Context::FrameIndex()];
    }
}