#include "flpch.h"
#include "ComputeCommandEncoder.h"

#include "Flourish/Backends/Vulkan/Context.h"
#include "Flourish/Backends/Vulkan/Buffer.h"
#include "Flourish/Backends/Vulkan/CommandBuffer.h"
#include "Flourish/Backends/Vulkan/ComputeTarget.h"

namespace Flourish::Vulkan
{
    ComputeCommandEncoder::ComputeCommandEncoder(CommandBuffer* parentBuffer, bool frameRestricted)
        : m_ParentBuffer(parentBuffer), m_FrameRestricted(frameRestricted)
    {}

    void ComputeCommandEncoder::BeginEncoding(ComputeTarget* target)
    {
        m_Encoding = true;
        m_BoundTarget = target;

        m_AllocInfo = Context::Commands().AllocateBuffers(
            GPUWorkloadType::Compute,
            false,
            &m_CommandBuffer,
            1, !m_FrameRestricted
        );   
    
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        // TODO: check result?
        vkBeginCommandBuffer(m_CommandBuffer, &beginInfo);
    }

    void ComputeCommandEncoder::EndEncoding()
    {
        FL_CRASH_ASSERT(m_Encoding, "Cannot end encoding that has already ended");
        m_Encoding = false;
        m_BoundTarget = nullptr;
        m_BoundDescriptorSet = nullptr;
        m_BoundPipeline = nullptr;

        VkCommandBuffer buffer = m_CommandBuffer;
        vkEndCommandBuffer(buffer);
        m_ParentBuffer->SubmitEncodedCommands(buffer, m_AllocInfo, GPUWorkloadType::Compute);
    }

    void ComputeCommandEncoder::BindPipeline(Flourish::ComputePipeline* pipeline)
    {
        if (m_BoundPipeline == pipeline) return;
        m_BoundPipeline = static_cast<ComputePipeline*>(pipeline);
        m_BoundDescriptorSet = m_BoundTarget->GetPipelineDescriptorSet(m_BoundPipeline);

        vkCmdBindPipeline(m_CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_BoundPipeline->GetPipeline());
    }
    
    void ComputeCommandEncoder::Dispatch(u32 x, u32 y, u32 z)
    {
        FL_CRASH_ASSERT(m_Encoding, "Cannot encode Dispatch after encoding has ended");

        vkCmdDispatch(m_CommandBuffer, x, y, z);
    }

    void ComputeCommandEncoder::DispatchIndirect(Flourish::Buffer* _buffer, u32 commandOffset)
    {
        FL_CRASH_ASSERT(m_Encoding, "Cannot encode DispatchIndirect after encoding has ended");

        VkBuffer buffer = static_cast<Buffer*>(_buffer)->GetBuffer();

        vkCmdDispatchIndirect(
            m_CommandBuffer,
            buffer,
            commandOffset * sizeof(VkDispatchIndirectCommand)
        );
    }
    
    void ComputeCommandEncoder::BindPipelineBufferResource(u32 bindingIndex, Flourish::Buffer* buffer, u32 bufferOffset, u32 dynamicOffset, u32 elementCount)
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

    void ComputeCommandEncoder::BindPipelineTextureResource(u32 bindingIndex, Flourish::Texture* texture)
    {
        ValidatePipelineBinding(bindingIndex, ShaderResourceType::Texture, texture);

        m_BoundDescriptorSet->UpdateBinding(
            bindingIndex, 
            ShaderResourceType::Texture, 
            texture,
            false, 0, 0
        );
    }

    void ComputeCommandEncoder::FlushPipelineBindings()
    {
        FL_CRASH_ASSERT(m_BoundPipeline, "Must call BindPipeline and bind all resources before FlushBindings");

        // Update a newly allocated descriptor set based on the current bindings or return
        // a cached one that was created before with the same binding info
        m_BoundDescriptorSet->FlushBindings();

        FL_CRASH_ASSERT(m_BoundDescriptorSet->GetMostRecentDescriptorSet() != nullptr);

        // Bind the new set
        VkDescriptorSet sets[1] = { m_BoundDescriptorSet->GetMostRecentDescriptorSet() };
        vkCmdBindDescriptorSets(
            m_CommandBuffer,
            VK_PIPELINE_BIND_POINT_COMPUTE,
            m_BoundPipeline->GetLayout(),
            0, 1,
            sets,
            static_cast<u32>(m_BoundDescriptorSet->GetLayout().GetDynamicOffsets().size()),
            m_BoundDescriptorSet->GetLayout().GetDynamicOffsets().data()
        );
    }

    void ComputeCommandEncoder::ValidatePipelineBinding(u32 bindingIndex, ShaderResourceType resourceType, void* resource)
    {
        FL_CRASH_ASSERT(m_BoundPipeline, "Must call BindPipeline before BindPipelineResource");
        FL_CRASH_ASSERT(resource != nullptr, "Cannot bind a null resource to a shader");

        if (!m_BoundDescriptorSet->GetLayout().DoesBindingExist(bindingIndex))
            return; // Silently ignore, TODO: warning once in the console when this happens

        FL_CRASH_ASSERT(m_BoundDescriptorSet->GetLayout().IsResourceCorrectType(bindingIndex, resourceType), "Attempting to bind a resource that does not match the bind index type");
    }
}