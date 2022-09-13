#include "flpch.h"
#include "RenderCommandEncoder.h"

#include "Flourish/Backends/Vulkan/Util/Context.h"
#include "Flourish/Backends/Vulkan/Framebuffer.h"
#include "Flourish/Backends/Vulkan/Buffer.h"

namespace Flourish::Vulkan
{
    RenderCommandEncoder::RenderCommandEncoder(const RenderCommandEncoderCreateInfo& createInfo)
        : Flourish::RenderCommandEncoder(createInfo)
    {
        m_AllocatedThread = std::this_thread::get_id();
        Context::Commands().AllocateBuffers(
            GPUWorkloadType::Graphics,
            true,
            m_CommandBuffers.data(),
            Flourish::Context::FrameBufferCount(),
            m_AllocatedThread
        );   
    }

    RenderCommandEncoder::~RenderCommandEncoder()
    {
        FL_ASSERT(
            m_AllocatedThread == std::this_thread::get_id(),
            "Render command encoder should never be destroyed from a thread different than the one that created it"
        );
        FL_ASSERT(
            Flourish::Context::IsThreadRegistered(),
            "Destroying render command encoder on deregistered thread, which will cause a memory leak"
        );

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

    void RenderCommandEncoder::BeginEncoding()
    {
        FL_CRASH_ASSERT(!m_Encoding, "Cannot begin render command encoding that has already begun");

        VkCommandBufferInheritanceInfo inheritanceInfo{};
        inheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = m_Info.Reusable ? VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT : VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        beginInfo.pInheritanceInfo = &inheritanceInfo;

        VkCommandBuffer buffer = m_CommandBuffers[Context::FrameIndex()];
        Framebuffer* fbuffer = static_cast<Framebuffer*>(m_Info.Framebuffer.get());
        
        // TODO: check result?
        vkBeginCommandBuffer(buffer, &beginInfo);

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
        
        m_Encoding = true;
    }

    void RenderCommandEncoder::EndEncoding()
    {
        FL_CRASH_ASSERT(m_Encoding, "Cannot end render command encoding that has not begun");
        
        VkCommandBuffer buffer = m_CommandBuffers[Context::FrameIndex()];
        
        vkCmdEndRenderPass(buffer);
        vkEndCommandBuffer(buffer);
        
        m_Encoding = false;
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

        vkCmdSetViewport(m_CommandBuffers[Context::FrameIndex()], 0, 1, &viewport);
    }

    void RenderCommandEncoder::BindVertexBuffer(Flourish::Buffer* _buffer)
    {
        VkBuffer buffer = static_cast<Buffer*>(_buffer)->GetBuffer();

        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(m_CommandBuffers[Context::FrameIndex()], 0, 1, &buffer, offsets);
    }

    void RenderCommandEncoder::BindIndexBuffer(Buffer* buffer)
    {

    }

    void RenderCommandEncoder::DrawIndexed(u32 indexCount, u32 indexOffset, u32 vertexOffset, u32 instanceCount)
    {
}

    void RenderCommandEncoder::Draw(u32 vertexCount, u32 vertexOffset, u32 instanceCount)
    {

    }
}