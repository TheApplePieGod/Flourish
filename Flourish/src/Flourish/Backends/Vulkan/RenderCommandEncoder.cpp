#include "flpch.h"
#include "RenderCommandEncoder.h"

#include "Flourish/Backends/Vulkan/Context.h"
#include "Flourish/Backends/Vulkan/Texture.h"
#include "Flourish/Backends/Vulkan/Framebuffer.h"
#include "Flourish/Backends/Vulkan/Buffer.h"
#include "Flourish/Backends/Vulkan/RenderPass.h"
#include "Flourish/Backends/Vulkan/CommandBuffer.h"
#include "Flourish/Backends/Vulkan/Shader.h"
#include "Flourish/Backends/Vulkan/DescriptorSet.h"

namespace Flourish::Vulkan
{
    RenderCommandEncoder::RenderCommandEncoder(CommandBuffer* parentBuffer, bool frameRestricted)
        : m_ParentBuffer(parentBuffer), m_FrameRestricted(frameRestricted)
    {}

    void RenderCommandEncoder::BeginEncoding(Framebuffer* framebuffer)
    {
        m_Encoding = true;
        m_BoundFramebuffer = framebuffer;

        m_AllocInfo = Context::Commands().AllocateBuffers(
            GPUWorkloadType::Graphics,
            false,
            &m_CommandBuffer,
            1, !m_FrameRestricted
        );   
    
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        // TODO: check result?
        vkBeginCommandBuffer(m_CommandBuffer, &beginInfo);

        VkRenderPassBeginInfo rpBeginInfo{};
        rpBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rpBeginInfo.renderPass = static_cast<RenderPass*>(framebuffer->GetRenderPass())->GetRenderPass();
        rpBeginInfo.framebuffer = framebuffer->GetFramebuffer();
        rpBeginInfo.renderArea.offset = { 0, 0 };
        rpBeginInfo.renderArea.extent = { framebuffer->GetWidth(), framebuffer->GetHeight() };
        rpBeginInfo.clearValueCount = static_cast<u32>(framebuffer->GetClearValues().size());
        rpBeginInfo.pClearValues = framebuffer->GetClearValues().data();
        vkCmdBeginRenderPass(m_CommandBuffer, &rpBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        SetViewport(0, 0, framebuffer->GetWidth(), framebuffer->GetHeight());
        SetScissor(0, 0, framebuffer->GetWidth(), framebuffer->GetHeight());
        SetLineWidth(1.f);
    }

    void RenderCommandEncoder::EndEncoding()
    {
        FL_CRASH_ASSERT(m_Encoding, "Cannot end encoding that has already ended");
        m_Encoding = false;
        m_BoundFramebuffer = nullptr;
        m_BoundPipeline = nullptr;
        m_BoundPipelineName.clear();
        m_SubpassIndex = 0;

        VkCommandBuffer buffer = m_CommandBuffer;
        vkCmdEndRenderPass(buffer);
        vkEndCommandBuffer(buffer);
        m_ParentBuffer->SubmitEncodedCommands(buffer, m_AllocInfo);
    }

    void RenderCommandEncoder::BindPipeline(const std::string_view pipelineName)
    {
        if (m_BoundPipelineName == pipelineName) return;
        m_BoundPipelineName = pipelineName;

        GraphicsPipeline* pipeline = static_cast<GraphicsPipeline*>(
            m_BoundFramebuffer->GetRenderPass()->GetPipeline(pipelineName).get()
        );
        FL_ASSERT(pipeline, "BindPipeline() pipeline not found");
        m_BoundPipeline = pipeline;

        m_ShaderRefs = {
            static_cast<const Shader*>(m_BoundPipeline->GetVertexShader()),
            static_cast<const Shader*>(m_BoundPipeline->GetFragmentShader())
        };

        for (u32 i = 0; i < m_DescriptorBinders.size(); i++)
            m_DescriptorBinders[i].BindNewShader(m_ShaderRefs[i]);

        VkPipeline subpassPipeline = pipeline->GetPipeline(m_SubpassIndex);
        FL_ASSERT(subpassPipeline, "BindPipeline() pipeline not supported for current subpass");

        vkCmdBindPipeline(m_CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, subpassPipeline);
    }

    void RenderCommandEncoder::SetViewport(u32 x, u32 y, u32 width, u32 height)
    {
        FL_CRASH_ASSERT(m_Encoding, "Cannot encode SetViewport after encoding has ended");

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (f32)width;
        viewport.height = (f32)height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        vkCmdSetViewport(m_CommandBuffer, 0, 1, &viewport);
    }

    void RenderCommandEncoder::SetScissor(u32 x, u32 y, u32 width, u32 height)
    {
        FL_CRASH_ASSERT(m_Encoding, "Cannot encode SetScissor after encoding has ended");

        VkRect2D scissor{};
        scissor.offset = { 0, 0 };
        scissor.extent = { width, height };

        vkCmdSetScissor(m_CommandBuffer, 0, 1, &scissor);
    }

    void RenderCommandEncoder::SetLineWidth(float width)
    {
        FL_ASSERT(width >= 0, "Width cannot be less than zero");
        FL_CRASH_ASSERT(m_Encoding, "Cannot encode SetLineWidth after encoding has ended");

        if (!Flourish::Context::FeatureTable().WideLines)
            width = std::min(width, 1.f);

        vkCmdSetLineWidth(m_CommandBuffer, width);
    }

    void RenderCommandEncoder::BindVertexBuffer(const Flourish::Buffer* _buffer)
    {
        FL_CRASH_ASSERT(m_Encoding, "Cannot encode BindVertexBuffer after encoding has ended");

        VkBuffer buffer = static_cast<const Buffer*>(_buffer)->GetBuffer();

        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(m_CommandBuffer, 0, 1, &buffer, offsets);
    }

    void RenderCommandEncoder::BindIndexBuffer(const Flourish::Buffer* _buffer)
    {
        FL_CRASH_ASSERT(m_Encoding, "Cannot encode BindIndexBuffer after encoding has ended");

        VkBuffer buffer = static_cast<const Buffer*>(_buffer)->GetBuffer();
        
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindIndexBuffer(m_CommandBuffer, buffer, 0, VK_INDEX_TYPE_UINT32);
    }

    void RenderCommandEncoder::Draw(u32 vertexCount, u32 vertexOffset, u32 instanceCount, u32 instanceOffset)
    {
        FL_CRASH_ASSERT(m_Encoding, "Cannot encode Draw after encoding has ended");
        
        vkCmdDraw(m_CommandBuffer, vertexCount, instanceCount, vertexOffset, instanceOffset);
    }

    void RenderCommandEncoder::DrawIndexed(u32 indexCount, u32 indexOffset, u32 vertexOffset, u32 instanceCount, u32 instanceOffset)
    {
        FL_CRASH_ASSERT(m_Encoding, "Cannot encode DrawIndexed after encoding has ended");

        vkCmdDrawIndexed(m_CommandBuffer, indexCount, instanceCount, indexOffset, vertexOffset, instanceOffset);
    }

    void RenderCommandEncoder::DrawIndexedIndirect(const Flourish::Buffer* _buffer, u32 commandOffset, u32 drawCount)
    {
        FL_CRASH_ASSERT(m_Encoding, "Cannot encode DrawIndexedIndirect after encoding has ended");

        VkBuffer buffer = static_cast<const Buffer*>(_buffer)->GetBuffer();

        u32 stride = _buffer->GetStride();
        vkCmdDrawIndexedIndirect(
            m_CommandBuffer,
            buffer,
            commandOffset * stride,
            drawCount,
            stride
        );
    }
    
    void RenderCommandEncoder::StartNextSubpass()
    {
        FL_CRASH_ASSERT(m_Encoding, "Cannot encode StartNextSubpass after encoding has ended");

        vkCmdNextSubpass(m_CommandBuffer, VK_SUBPASS_CONTENTS_INLINE);

        // Pipeline must be reset between each subpass
        m_SubpassIndex++;
        m_BoundPipeline = nullptr;
        m_BoundPipelineName.clear();
    }

    void RenderCommandEncoder::ClearColorAttachment(u32 attachmentIndex)
    {
        FL_CRASH_ASSERT(m_Encoding, "Cannot encode ClearColorAttachment after encoding has ended");
        
        auto& color = m_BoundFramebuffer->GetColorAttachments()[attachmentIndex].ClearColor;

        VkClearAttachment clear;
        clear.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        clear.clearValue.color.float32[0] = color[0];
        clear.clearValue.color.float32[1] = color[1];
        clear.clearValue.color.float32[2] = color[2];
        clear.clearValue.color.float32[3] = color[3];
        clear.colorAttachment = attachmentIndex;
        VkClearRect clearRect;
        clearRect.baseArrayLayer = 0;
        clearRect.layerCount = 1;
        clearRect.rect.extent.width = m_BoundFramebuffer->GetWidth();
        clearRect.rect.extent.height = m_BoundFramebuffer->GetHeight();
        clearRect.rect.offset.x = 0;
        clearRect.rect.offset.y = 0;
        vkCmdClearAttachments(m_CommandBuffer, 1, &clear, 1, &clearRect);        
    }

    void RenderCommandEncoder::ClearDepthAttachment()
    {
        FL_CRASH_ASSERT(m_Encoding, "Cannot encode ClearDepthAttachment after encoding has ended");
        
        VkClearAttachment clear;
        clear.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        clear.clearValue.depthStencil.depth = Flourish::Context::ReversedZBuffer() ? 0.f : 1.0f;
        clear.clearValue.depthStencil.stencil = 0;
        VkClearRect clearRect;
        clearRect.baseArrayLayer = 0;
        clearRect.layerCount = 1;
        clearRect.rect.extent.height = m_BoundFramebuffer->GetWidth();
        clearRect.rect.extent.width = m_BoundFramebuffer->GetHeight();
        clearRect.rect.offset.x = 0;
        clearRect.rect.offset.y = 0;
        vkCmdClearAttachments(m_CommandBuffer, 1, &clear, 1, &clearRect);        
    }

    void RenderCommandEncoder::BindDescriptorSet(const Flourish::DescriptorSet* set, u32 setIndex)
    {
        FL_CRASH_ASSERT(m_BoundPipeline, "Must call BindPipeline before binding a descriptor set");

        for (u32 i = 0; i < m_DescriptorBinders.size(); i++)
        {
            if (m_ShaderRefs[i]->DoesSetExist(setIndex))
            {
                m_DescriptorBinders[i].BindDescriptorSet(static_cast<const DescriptorSet*>(set), setIndex);
                return;
            }
        }

        FL_CRASH_ASSERT(false, "Set index does not exist in shader");
    }

    void RenderCommandEncoder::UpdateDynamicOffset(u32 setIndex, u32 bindingIndex, u32 offset)
    {
        FL_CRASH_ASSERT(m_BoundPipeline, "Must call BindPipeline before updating dynamic offsets");

        for (u32 i = 0; i < m_DescriptorBinders.size(); i++)
        {
            if (m_ShaderRefs[i]->DoesSetExist(setIndex))
            {
                m_DescriptorBinders[i].UpdateDynamicOffset(setIndex, bindingIndex, offset);
                return;
            }
        }

        FL_CRASH_ASSERT(false, "Set index does not exist in shader");
    }

    void RenderCommandEncoder::FlushDescriptorSet(u32 setIndex)
    {
        FL_CRASH_ASSERT(m_BoundPipeline, "Must call BindPipeline before flushing a descriptor set");

        for (u32 i = 0; i < m_DescriptorBinders.size(); i++)
        {
            if (m_ShaderRefs[i]->DoesSetExist(setIndex))
            {
                // TODO: ensure bound

                VkDescriptorSet sets[1] = { m_DescriptorBinders[i].GetDescriptorSet(setIndex)->GetSet() };
                vkCmdBindDescriptorSets(
                    m_CommandBuffer,
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    m_BoundPipeline->GetLayout(),
                    setIndex, 1,
                    sets,
                    m_ShaderRefs[i]->GetSetData()[setIndex].DynamicOffsetCount,
                    m_DescriptorBinders[i].GetDynamicOffsetData(setIndex)
                );

                return;
            }
        }

        FL_CRASH_ASSERT(false, "Set index does not exist in shader");
    }
}
