#include "flpch.h"
#include "ComputeCommandEncoder.h"

#include "Flourish/Backends/Vulkan/Context.h"
#include "Flourish/Backends/Vulkan/Buffer.h"
#include "Flourish/Backends/Vulkan/CommandBuffer.h"
#include "Flourish/Backends/Vulkan/ResourceSet.h"
#include "Flourish/Backends/Vulkan/Shader.h"

namespace Flourish::Vulkan
{
    ComputeCommandEncoder::ComputeCommandEncoder(CommandBuffer* parentBuffer, bool frameRestricted)
        : m_ParentBuffer(parentBuffer), m_FrameRestricted(frameRestricted)
    {}

    void ComputeCommandEncoder::BeginEncoding()
    {
        m_Encoding = true;

        m_Submission.Buffers.resize(1);
        m_Submission.AllocInfo = Context::Commands().AllocateBuffers(
            GPUWorkloadType::Compute,
            true,
            m_Submission.Buffers.data(),
            m_Submission.Buffers.size(),
            !m_FrameRestricted
        );   
        m_CommandBuffer = m_Submission.Buffers[0];

        VkCommandBufferInheritanceInfo inheritanceInfo{};
        inheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
    
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        beginInfo.pInheritanceInfo = &inheritanceInfo;

        // TODO: check result?
        vkBeginCommandBuffer(m_CommandBuffer, &beginInfo);

        m_DescriptorBinder.Reset();
    }

    void ComputeCommandEncoder::EndEncoding()
    {
        FL_CRASH_ASSERT(m_Encoding, "Cannot end encoding that has already ended");
        m_Encoding = false;
        m_BoundPipeline = nullptr;

        vkEndCommandBuffer(m_CommandBuffer);
        m_ParentBuffer->SubmitEncodedCommands(m_Submission);
    }

    void ComputeCommandEncoder::BindPipeline(Flourish::ComputePipeline* pipeline)
    {
        if (m_BoundPipeline == static_cast<ComputePipeline*>(pipeline)) return;
        m_BoundPipeline = static_cast<ComputePipeline*>(pipeline);

        m_DescriptorBinder.BindPipelineData(m_BoundPipeline->GetDescriptorData());

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
    
    void ComputeCommandEncoder::BindResourceSet(const Flourish::ResourceSet* set, u32 setIndex)
    {
        FL_CRASH_ASSERT(m_BoundPipeline, "Must call BindPipeline before binding a resource set");

        m_DescriptorBinder.BindResourceSet(static_cast<const ResourceSet*>(set), setIndex);
    }

    void ComputeCommandEncoder::UpdateDynamicOffset(u32 setIndex, u32 bindingIndex, u32 offset)
    {
        FL_CRASH_ASSERT(m_BoundPipeline, "Must call BindPipeline before updating dynamic offsets");

        m_DescriptorBinder.UpdateDynamicOffset(setIndex, bindingIndex, offset);
    }

    void ComputeCommandEncoder::FlushResourceSet(u32 setIndex)
    {
        FL_CRASH_ASSERT(m_BoundPipeline, "Must call BindPipeline before flushing a resource set");

        auto shader = static_cast<const Shader*>(m_BoundPipeline->GetComputeShader());

        auto set = m_DescriptorBinder.GetResourceSet(setIndex);
        VkDescriptorSet sets[1] = { set->GetSet() };
        vkCmdBindDescriptorSets(
            m_CommandBuffer,
            VK_PIPELINE_BIND_POINT_COMPUTE,
            m_BoundPipeline->GetLayout(),
            setIndex, 1,
            sets,
            m_DescriptorBinder.GetDynamicOffsetCount(setIndex),
            m_DescriptorBinder.GetDynamicOffsetData(setIndex)
        );
    }
}
