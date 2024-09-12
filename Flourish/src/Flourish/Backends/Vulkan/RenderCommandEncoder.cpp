#include "flpch.h"
#include "RenderCommandEncoder.h"

#include "Flourish/Backends/Vulkan/Context.h"
#include "Flourish/Backends/Vulkan/Texture.h"
#include "Flourish/Backends/Vulkan/Framebuffer.h"
#include "Flourish/Backends/Vulkan/Buffer.h"
#include "Flourish/Backends/Vulkan/RenderPass.h"
#include "Flourish/Backends/Vulkan/CommandBuffer.h"
#include "Flourish/Backends/Vulkan/Shader.h"
#include "Flourish/Backends/Vulkan/ResourceSet.h"

namespace Flourish::Vulkan
{
    RenderCommandEncoder::RenderCommandEncoder(CommandBuffer* parentBuffer, bool frameRestricted)
        : m_ParentBuffer(parentBuffer), m_FrameRestricted(frameRestricted)
    {}

    void RenderCommandEncoder::BeginEncoding(Framebuffer* framebuffer)
    {
        FL_PROFILE_FUNCTION();

        m_Encoding = true;
        m_AnyCommandRecorded = false;
        m_Submission.Framebuffer = framebuffer;
        m_Submission.Buffers.resize(framebuffer->GetRenderPass()->GetSubpasses().size());
        m_Submission.AllocInfo = Context::Commands().AllocateBuffers(
            GPUWorkloadType::Graphics,
            true,
            m_Submission.Buffers.data(),
            m_Submission.Buffers.size(),
            !m_FrameRestricted
        );   

        InitializeSubpass();
    }

    void RenderCommandEncoder::EndEncoding()
    {
        FL_PROFILE_FUNCTION();

        FL_CRASH_ASSERT(m_Encoding, "Cannot end encoding that has already ended");
        m_Encoding = false;
        m_BoundPipeline = nullptr;
        m_BoundPipelineName.clear();
        m_SubpassIndex = 0;

        vkEndCommandBuffer(m_CurrentCommandBuffer);

        // Indicate that we should do nothing here
        if (!m_AnyCommandRecorded)
            m_Submission.Buffers.clear();

        m_ParentBuffer->SubmitEncodedCommands(m_Submission);

        m_Submission.Framebuffer = nullptr;
    }

    void RenderCommandEncoder::BindPipeline(const std::string_view pipelineName)
    {
        FL_PROFILE_FUNCTION();

        if (m_BoundPipelineName == pipelineName) return;
        m_BoundPipelineName = pipelineName;

        GraphicsPipeline* pipeline = static_cast<GraphicsPipeline*>(
            m_Submission.Framebuffer->GetRenderPass()->GetPipeline(pipelineName)
        );
        FL_ASSERT(pipeline, "BindPipeline() pipeline not found");
        m_BoundPipeline = pipeline;

        // TODO: compile-time constant for enabling this feature
        pipeline->ValidateShaders();

        m_DescriptorBinder.BindPipelineData(m_BoundPipeline->GetDescriptorData());

        VkPipeline subpassPipeline = pipeline->GetPipeline(m_SubpassIndex);
        FL_ASSERT(subpassPipeline, "BindPipeline() pipeline not supported for current subpass");

        vkCmdBindPipeline(m_CurrentCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, subpassPipeline);
    }

    void RenderCommandEncoder::SetViewport(u32 x, u32 y, u32 width, u32 height)
    {
        FL_PROFILE_FUNCTION();

        FL_CRASH_ASSERT(m_Encoding, "Cannot encode SetViewport after encoding has ended");

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (f32)width;
        viewport.height = (f32)height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        vkCmdSetViewport(m_CurrentCommandBuffer, 0, 1, &viewport);
    }

    void RenderCommandEncoder::SetScissor(u32 x, u32 y, u32 width, u32 height)
    {
        FL_PROFILE_FUNCTION();
        
        FL_CRASH_ASSERT(m_Encoding, "Cannot encode SetScissor after encoding has ended");

        VkRect2D scissor{};
        scissor.offset = { 0, 0 };
        scissor.extent = { width, height };

        vkCmdSetScissor(m_CurrentCommandBuffer, 0, 1, &scissor);
    }

    void RenderCommandEncoder::SetLineWidth(float width)
    {
        FL_PROFILE_FUNCTION();

        FL_ASSERT(width >= 0, "Width cannot be less than zero");
        FL_CRASH_ASSERT(m_Encoding, "Cannot encode SetLineWidth after encoding has ended");

        if (!Flourish::Context::FeatureTable().WideLines)
            width = std::min(width, 1.f);

        vkCmdSetLineWidth(m_CurrentCommandBuffer, width);
    }

    void RenderCommandEncoder::BindVertexBuffer(const Flourish::Buffer* _buffer)
    {
        FL_PROFILE_FUNCTION();

        FL_CRASH_ASSERT(m_Encoding, "Cannot encode BindVertexBuffer after encoding has ended");
        FL_CRASH_ASSERT(_buffer->GetUsage() & BufferUsageFlags::Vertex, "BindVertexBuffer buffer must be created with 'Vertex' usage");

        VkBuffer buffer = static_cast<const Buffer*>(_buffer)->GetGPUBuffer();

        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(m_CurrentCommandBuffer, 0, 1, &buffer, offsets);
    }

    void RenderCommandEncoder::BindIndexBuffer(const Flourish::Buffer* _buffer)
    {
        FL_PROFILE_FUNCTION();

        FL_CRASH_ASSERT(m_Encoding, "Cannot encode BindIndexBuffer after encoding has ended");
        FL_CRASH_ASSERT(_buffer->GetUsage() & BufferUsageFlags::Index, "BindIndexBuffer buffer must be created with 'Index' usage");

        VkBuffer buffer = static_cast<const Buffer*>(_buffer)->GetGPUBuffer();
        
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindIndexBuffer(m_CurrentCommandBuffer, buffer, 0, VK_INDEX_TYPE_UINT32);
    }

    void RenderCommandEncoder::Draw(u32 vertexCount, u32 vertexOffset, u32 instanceCount, u32 instanceOffset)
    {
        FL_PROFILE_FUNCTION();

        FL_CRASH_ASSERT(m_Encoding, "Cannot encode Draw after encoding has ended");
        
        vkCmdDraw(m_CurrentCommandBuffer, vertexCount, instanceCount, vertexOffset, instanceOffset);
        m_AnyCommandRecorded = true;
    }

    void RenderCommandEncoder::DrawIndexed(u32 indexCount, u32 indexOffset, u32 vertexOffset, u32 instanceCount, u32 instanceOffset)
    {
        FL_PROFILE_FUNCTION();
        
        FL_CRASH_ASSERT(m_Encoding, "Cannot encode DrawIndexed after encoding has ended");

        vkCmdDrawIndexed(m_CurrentCommandBuffer, indexCount, instanceCount, indexOffset, vertexOffset, instanceOffset);
        m_AnyCommandRecorded = true;
    }

    void RenderCommandEncoder::DrawIndexedIndirect(const Flourish::Buffer* _buffer, u32 commandOffset, u32 drawCount)
    {
        FL_PROFILE_FUNCTION();

        FL_CRASH_ASSERT(m_Encoding, "Cannot encode DrawIndexedIndirect after encoding has ended");
        FL_CRASH_ASSERT(_buffer->GetUsage() & BufferUsageFlags::Indirect, "DrawIndexedIndirect buffer must be created with 'Indirect' usage");

        VkBuffer buffer = static_cast<const Buffer*>(_buffer)->GetGPUBuffer();

        u32 stride = _buffer->GetStride();
        vkCmdDrawIndexedIndirect(
            m_CurrentCommandBuffer,
            buffer,
            commandOffset * stride,
            drawCount,
            stride
        );
        m_AnyCommandRecorded = true;
    }
    
    void RenderCommandEncoder::StartNextSubpass()
    {
        FL_PROFILE_FUNCTION();

        FL_CRASH_ASSERT(m_Encoding, "Cannot encode StartNextSubpass after encoding has ended");

        vkEndCommandBuffer(m_CurrentCommandBuffer);

        // Pipeline must be reset between each subpass
        m_SubpassIndex++;
        m_BoundPipeline = nullptr;
        m_BoundPipelineName.clear();

        InitializeSubpass();
    }

    void RenderCommandEncoder::ClearColorAttachment(u32 attachmentIndex)
    {
        FL_PROFILE_FUNCTION();

        FL_CRASH_ASSERT(m_Encoding, "Cannot encode ClearColorAttachment after encoding has ended");
        
        auto& color = m_Submission.Framebuffer->GetColorAttachments()[attachmentIndex].ClearColor;

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
        clearRect.rect.extent.width = m_Submission.Framebuffer->GetWidth();
        clearRect.rect.extent.height = m_Submission.Framebuffer->GetHeight();
        clearRect.rect.offset.x = 0;
        clearRect.rect.offset.y = 0;
        vkCmdClearAttachments(m_CurrentCommandBuffer, 1, &clear, 1, &clearRect);        
        m_AnyCommandRecorded = true;
    }

    void RenderCommandEncoder::ClearDepthAttachment()
    {
        FL_PROFILE_FUNCTION();

        FL_CRASH_ASSERT(m_Encoding, "Cannot encode ClearDepthAttachment after encoding has ended");
        
        VkClearAttachment clear;
        clear.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        clear.clearValue.depthStencil.depth = Flourish::Context::ReversedZBuffer() ? 0.f : 1.0f;
        clear.clearValue.depthStencil.stencil = 0;
        VkClearRect clearRect;
        clearRect.baseArrayLayer = 0;
        clearRect.layerCount = 1;
        clearRect.rect.extent.width = m_Submission.Framebuffer->GetWidth();
        clearRect.rect.extent.height = m_Submission.Framebuffer->GetHeight();
        clearRect.rect.offset.x = 0;
        clearRect.rect.offset.y = 0;
        vkCmdClearAttachments(m_CurrentCommandBuffer, 1, &clear, 1, &clearRect);        
        m_AnyCommandRecorded = true;
    }

    void RenderCommandEncoder::BindResourceSet(const Flourish::ResourceSet* set, u32 setIndex)
    {
        FL_PROFILE_FUNCTION();

        FL_CRASH_ASSERT(m_BoundPipeline, "Must call BindPipeline before binding a resource set");
        FL_CRASH_ASSERT(m_DescriptorBinder.DoesSetExist(setIndex), "Set index does not exist in shader");

        m_DescriptorBinder.BindResourceSet(static_cast<const ResourceSet*>(set), setIndex);
    }

    void RenderCommandEncoder::UpdateDynamicOffset(u32 setIndex, u32 bindingIndex, u32 offset)
    {
        FL_PROFILE_FUNCTION();

        FL_CRASH_ASSERT(m_BoundPipeline, "Must call BindPipeline before updating dynamic offsets");

        if (m_DescriptorBinder.DoesSetExist(setIndex))
        {
            m_DescriptorBinder.UpdateDynamicOffset(setIndex, bindingIndex, offset);
            return;
        }

        FL_CRASH_ASSERT(false, "Set index does not exist in shader");
    }

    void RenderCommandEncoder::FlushResourceSet(u32 setIndex)
    {
        FL_PROFILE_FUNCTION();

        FL_CRASH_ASSERT(m_BoundPipeline, "Must call BindPipeline before flushing a resource set");
        FL_CRASH_ASSERT(m_DescriptorBinder.DoesSetExist(setIndex), "Set index does not exist in shader");

        // TODO: ensure bound
        auto set = m_DescriptorBinder.GetResourceSet(setIndex);
        VkDescriptorSet sets[1] = { set->GetSet() };
        vkCmdBindDescriptorSets(
            m_CurrentCommandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_BoundPipeline->GetLayout(),
            setIndex, 1,
            sets,
            m_DescriptorBinder.GetDynamicOffsetCount(setIndex),
            m_DescriptorBinder.GetDynamicOffsetData(setIndex)
        );
    }

    void RenderCommandEncoder::PushConstants(u32 offset, u32 size, const void* data)
    {
        FL_CRASH_ASSERT(m_BoundPipeline, "Must bind a pipeline before pushing constants");
        FL_ASSERT(
            size <= m_DescriptorBinder.GetBoundData()->PushConstantRange.size,
            "Push constant size out of range"
        );

        VkPipelineLayout layout = m_BoundPipeline->GetLayout();
        vkCmdPushConstants(
            m_CurrentCommandBuffer,
            layout,
            m_DescriptorBinder.GetBoundData()->PushConstantRange.stageFlags,
            offset,
            size,
            data
        );
    }

    void RenderCommandEncoder::InitializeSubpass()
    {
        m_CurrentCommandBuffer = m_Submission.Buffers[m_SubpassIndex];

        // TODO: store this in the class since its basically the same each time
        VkCommandBufferInheritanceInfo inheritanceInfo{};
        inheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
        inheritanceInfo.renderPass = static_cast<RenderPass*>(m_Submission.Framebuffer->GetRenderPass())->GetRenderPass();
        inheritanceInfo.subpass = m_SubpassIndex;
        inheritanceInfo.framebuffer = m_Submission.Framebuffer->GetFramebuffer();
    
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
        beginInfo.pInheritanceInfo = &inheritanceInfo;

        // TODO: check result?
        vkBeginCommandBuffer(m_CurrentCommandBuffer, &beginInfo);

        SetViewport(0, 0, m_Submission.Framebuffer->GetWidth(), m_Submission.Framebuffer->GetHeight());
        SetScissor(0, 0, m_Submission.Framebuffer->GetWidth(), m_Submission.Framebuffer->GetHeight());
        SetLineWidth(1.f);
    }
}
