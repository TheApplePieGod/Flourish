#include "flpch.h"
#include "RenderCommandEncoder.h"

#include "Flourish/Backends/Vulkan/Context.h"
#include "Flourish/Backends/Vulkan/Texture.h"
#include "Flourish/Backends/Vulkan/Framebuffer.h"
#include "Flourish/Backends/Vulkan/Buffer.h"
#include "Flourish/Backends/Vulkan/RenderPass.h"
#include "Flourish/Backends/Vulkan/CommandBuffer.h"

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
        m_BoundDescriptorSet = nullptr;
        m_BoundPipeline = nullptr;
        m_BoundPipelineName.clear();
        m_SubpassIndex = 0;

        VkCommandBuffer buffer = m_CommandBuffer;
        vkCmdEndRenderPass(buffer);
        vkEndCommandBuffer(buffer);
        m_ParentBuffer->SubmitEncodedCommands(buffer, m_AllocInfo, GPUWorkloadType::Graphics);
    }

    void RenderCommandEncoder::BindPipeline(const std::string_view pipelineName)
    {
        if (m_BoundPipelineName == pipelineName) return;
        m_BoundPipelineName = pipelineName;

        GraphicsPipeline* pipeline = static_cast<GraphicsPipeline*>(
            m_BoundFramebuffer->GetRenderPass()->GetPipeline(pipelineName).get()
        );
        m_BoundPipeline = pipeline;
        m_BoundDescriptorSet = m_BoundFramebuffer->GetPipelineDescriptorSet(pipelineName, pipeline->GetDescriptorSetLayout());

        vkCmdBindPipeline(m_CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->GetPipeline(m_SubpassIndex));
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

    void RenderCommandEncoder::BindVertexBuffer(Flourish::Buffer* _buffer)
    {
        FL_CRASH_ASSERT(m_Encoding, "Cannot encode BindVertexBuffer after encoding has ended");

        VkBuffer buffer = static_cast<Buffer*>(_buffer)->GetBuffer();

        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(m_CommandBuffer, 0, 1, &buffer, offsets);
    }

    void RenderCommandEncoder::BindIndexBuffer(Flourish::Buffer* _buffer)
    {
        FL_CRASH_ASSERT(m_Encoding, "Cannot encode BindIndexBuffer after encoding has ended");

        VkBuffer buffer = static_cast<Buffer*>(_buffer)->GetBuffer();
        
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindIndexBuffer(m_CommandBuffer, buffer, 0, VK_INDEX_TYPE_UINT32);
    }

    void RenderCommandEncoder::Draw(u32 vertexCount, u32 vertexOffset, u32 instanceCount)
    {
        FL_CRASH_ASSERT(m_Encoding, "Cannot encode Draw after encoding has ended");
        
        vkCmdDraw(m_CommandBuffer, vertexCount, instanceCount, vertexOffset, 0);
    }

    void RenderCommandEncoder::DrawIndexed(u32 indexCount, u32 indexOffset, u32 vertexOffset, u32 instanceCount)
    {
        FL_CRASH_ASSERT(m_Encoding, "Cannot encode DrawIndexed after encoding has ended");

        vkCmdDrawIndexed(m_CommandBuffer, indexCount, instanceCount, indexOffset, vertexOffset, 0);
    }

    void RenderCommandEncoder::DrawIndexedIndirect(Flourish::Buffer* _buffer, u32 commandOffset, u32 drawCount)
    {
        FL_CRASH_ASSERT(m_Encoding, "Cannot encode DrawIndexedIndirect after encoding has ended");

        VkBuffer buffer = static_cast<Buffer*>(_buffer)->GetBuffer();

        vkCmdDrawIndexedIndirect(
            m_CommandBuffer,
            buffer,
            commandOffset * _buffer->GetLayout().GetStride(),
            drawCount,
            _buffer->GetLayout().GetStride()
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
        m_BoundDescriptorSet = nullptr;
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

    void RenderCommandEncoder::BindPipelineBufferResource(u32 bindingIndex, Flourish::Buffer* buffer, u32 bufferOffset, u32 dynamicOffset, u32 elementCount)
    {
        FL_CRASH_ASSERT(elementCount + dynamicOffset + bufferOffset <= buffer->GetAllocatedCount(), "ElementCount + BufferOffset + DynamicOffset must be <= buffer allocated count");
        FL_CRASH_ASSERT(buffer->GetType() == BufferType::Uniform || buffer->GetType() == BufferType::Storage, "Buffer bind must be either a uniform or storage buffer");

        ShaderResourceType bufferType = buffer->GetType() == BufferType::Uniform ? ShaderResourceType::UniformBuffer : ShaderResourceType::StorageBuffer;
        ValidatePipelineBinding(bindingIndex, bufferType, buffer);

        m_BoundDescriptorSet->UpdateDynamicOffset(bindingIndex, dynamicOffset * buffer->GetLayout().GetStride());
        m_BoundDescriptorSet->UpdateBinding(
            bindingIndex, 
            bufferType, 
            buffer,
            true,
            buffer->GetLayout().GetStride() * bufferOffset,
            buffer->GetLayout().GetStride() * elementCount
        );
    }

    void RenderCommandEncoder::BindPipelineTextureResource(u32 bindingIndex, Flourish::Texture* texture)
    {
        // Ensure the texture to bind is not an output attachment
        FL_CRASH_ASSERT(
            std::find_if(
                m_BoundFramebuffer->GetColorAttachments().begin(),
                m_BoundFramebuffer->GetColorAttachments().end(),
                [texture](const FramebufferColorAttachment& att){ return att.Texture.get() == texture; }
            ) == m_BoundFramebuffer->GetColorAttachments().end(),
            "Cannot bind a texture resource that is currently being written to"
        );

        ShaderResourceType texType = texture->GetUsageType() == TextureUsageType::ComputeTarget ? ShaderResourceType::StorageTexture : ShaderResourceType::Texture;
        
        ValidatePipelineBinding(bindingIndex, texType, texture);
        FL_ASSERT(
            m_BoundDescriptorSet->GetLayout().GetBindingType(bindingIndex) != ShaderResourceType::StorageTexture || texType == ShaderResourceType::StorageTexture,
            "Attempting to bind a texture to a storage image binding that was not created as a compute target"
        );

        m_BoundDescriptorSet->UpdateBinding(
            bindingIndex, 
            texType, 
            texture,
            false, 0, 0
        );
    }
    
    void RenderCommandEncoder::BindPipelineTextureLayerResource(u32 bindingIndex, Flourish::Texture* texture, u32 layerIndex, u32 mipLevel)
    {
        // Ensure the texture to bind is not an output attachment
        FL_CRASH_ASSERT(
            std::find_if(
                m_BoundFramebuffer->GetColorAttachments().begin(),
                m_BoundFramebuffer->GetColorAttachments().end(),
                [texture, layerIndex, mipLevel](const FramebufferColorAttachment& att)
                { return att.Texture.get() == texture && att.LayerIndex == layerIndex && att.MipLevel == mipLevel; }
            ) == m_BoundFramebuffer->GetColorAttachments().end(),
            "Cannot bind a texture resource that is currently being written to"
        )
        
        ShaderResourceType texType = texture->GetUsageType() == TextureUsageType::ComputeTarget ? ShaderResourceType::StorageTexture : ShaderResourceType::Texture;

        ValidatePipelineBinding(bindingIndex, texType, texture);
        FL_ASSERT(
            m_BoundDescriptorSet->GetLayout().GetBindingType(bindingIndex) != ShaderResourceType::StorageTexture || texType == ShaderResourceType::StorageTexture,
            "Attempting to bind a texture to a storage image binding that was not created as a compute target"
        );

        m_BoundDescriptorSet->UpdateBinding(
            bindingIndex, 
            texType, 
            texture,
            true, layerIndex, mipLevel
        );
    }

    void RenderCommandEncoder::BindPipelineSubpassInputResource(u32 bindingIndex, SubpassAttachment attachment)
    {
        ValidatePipelineBinding(bindingIndex, ShaderResourceType::SubpassInput, &attachment);
        
        VkImageView attView = m_BoundFramebuffer->GetAttachmentImageView(attachment);

        m_BoundDescriptorSet->UpdateBinding(
            bindingIndex, 
            ShaderResourceType::SubpassInput, 
            attView,
            attachment.Type == SubpassAttachmentType::Color, 0, 0
        );
    }

    void RenderCommandEncoder::FlushPipelineBindings()
    {
        FL_CRASH_ASSERT(!m_BoundPipelineName.empty(), "Must call BindPipeline and bind all resources before FlushBindings");

        // Update a newly allocated descriptor set based on the current bindings or return
        // a cached one that was created before with the same binding info
        m_BoundDescriptorSet->FlushBindings();

        FL_CRASH_ASSERT(m_BoundDescriptorSet->GetMostRecentDescriptorSet() != nullptr);

        // Bind the new set
        VkDescriptorSet sets[1] = { m_BoundDescriptorSet->GetMostRecentDescriptorSet() };
        vkCmdBindDescriptorSets(
            m_CommandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_BoundPipeline->GetLayout(),
            0, 1,
            sets,
            static_cast<u32>(m_BoundDescriptorSet->GetLayout().GetDynamicOffsets().size()),
            m_BoundDescriptorSet->GetLayout().GetDynamicOffsets().data()
        );
    }
    
    void RenderCommandEncoder::ValidatePipelineBinding(u32 bindingIndex, ShaderResourceType resourceType, void* resource)
    {
        FL_CRASH_ASSERT(!m_BoundPipelineName.empty(), "Must call BindPipeline before BindPipelineResource");
        FL_CRASH_ASSERT(resource != nullptr, "Cannot bind a null resource to a shader");

        if (!m_BoundDescriptorSet->GetLayout().DoesBindingExist(bindingIndex))
            return; // Silently ignore, TODO: warning once in the console when this happens

        FL_CRASH_ASSERT(m_BoundDescriptorSet->GetLayout().IsResourceCorrectType(bindingIndex, resourceType), "Attempting to bind a resource that does not match the bind index type");
    }
}