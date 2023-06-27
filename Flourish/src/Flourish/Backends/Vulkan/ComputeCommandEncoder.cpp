#include "flpch.h"
#include "ComputeCommandEncoder.h"

#include "Flourish/Backends/Vulkan/Context.h"
#include "Flourish/Backends/Vulkan/Buffer.h"
#include "Flourish/Backends/Vulkan/CommandBuffer.h"
#include "Flourish/Backends/Vulkan/ResourceSet.h"
#include "Flourish/Backends/Vulkan/Shader.h"
#include "Flourish/Backends/Vulkan/ComputePipeline.h"
#include "Flourish/Backends/Vulkan/RayTracing/RayTracingPipeline.h"
#include "Flourish/Backends/Vulkan/RayTracing/RayTracingGroupTable.h"
#include "Flourish/Backends/Vulkan/RayTracing/AccelerationStructure.h"

namespace Flourish::Vulkan
{
    ComputeCommandEncoder::ComputeCommandEncoder(CommandBuffer* parentBuffer, bool frameRestricted)
        : m_ParentBuffer(parentBuffer), m_FrameRestricted(frameRestricted)
    {}

    void ComputeCommandEncoder::BeginEncoding()
    {
        m_Encoding = true;
        m_AnyCommandRecorded = false;

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
        m_BoundComputePipeline = nullptr;
        m_BoundRayTracingPipeline = nullptr;

        vkEndCommandBuffer(m_CommandBuffer);

        if (!m_AnyCommandRecorded)
            m_Submission.Buffers.clear();

        m_ParentBuffer->SubmitEncodedCommands(m_Submission);
    }

    void ComputeCommandEncoder::BindComputePipeline(Flourish::ComputePipeline* pipeline)
    {
        if (m_BoundComputePipeline == static_cast<ComputePipeline*>(pipeline)) return;
        m_BoundComputePipeline = static_cast<ComputePipeline*>(pipeline);
        m_BoundRayTracingPipeline = nullptr;

        m_DescriptorBinder.BindPipelineData(m_BoundComputePipeline->GetDescriptorData());

        vkCmdBindPipeline(m_CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_BoundComputePipeline->GetPipeline());
    }
    
    void ComputeCommandEncoder::Dispatch(u32 x, u32 y, u32 z)
    {
        FL_CRASH_ASSERT(m_Encoding, "Cannot encode Dispatch after encoding has ended");
        FL_CRASH_ASSERT(m_BoundComputePipeline, "Must bind compute pipeline before dispatching");

        vkCmdDispatch(m_CommandBuffer, x, y, z);
        m_AnyCommandRecorded = true;
    }

    void ComputeCommandEncoder::DispatchIndirect(Flourish::Buffer* _buffer, u32 commandOffset)
    {
        FL_CRASH_ASSERT(m_Encoding, "Cannot encode DispatchIndirect after encoding has ended");
        FL_CRASH_ASSERT(m_BoundComputePipeline, "Must bind compute pipeline before dispatching");

        VkBuffer buffer = static_cast<Buffer*>(_buffer)->GetBuffer();

        vkCmdDispatchIndirect(
            m_CommandBuffer,
            buffer,
            commandOffset * sizeof(VkDispatchIndirectCommand)
        );
        m_AnyCommandRecorded = true;
    }

    void ComputeCommandEncoder::BindRayTracingPipeline(Flourish::RayTracingPipeline* pipeline)
    {
        if (m_BoundRayTracingPipeline == static_cast<RayTracingPipeline*>(pipeline)) return;
        m_BoundRayTracingPipeline = static_cast<RayTracingPipeline*>(pipeline);
        m_BoundComputePipeline = nullptr;

        m_DescriptorBinder.BindPipelineData(m_BoundRayTracingPipeline->GetDescriptorData());

        vkCmdBindPipeline(m_CommandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_BoundRayTracingPipeline->GetPipeline());
    }

    void ComputeCommandEncoder::TraceRays(Flourish::RayTracingGroupTable* _groupTable, u32 width, u32 height, u32 depth)
    {
        FL_CRASH_ASSERT(m_BoundRayTracingPipeline, "Must bind ray tracing pipeline before tracing rays");

        RayTracingGroupTable* table = static_cast<RayTracingGroupTable*>(_groupTable);
        std::array<VkStridedDeviceAddressRegionKHR, 4> regions = {
            table->GetBufferRegion(RayTracingShaderGroupType::RayGen),
            table->GetBufferRegion(RayTracingShaderGroupType::Miss),
            table->GetBufferRegion(RayTracingShaderGroupType::Hit),
            table->GetBufferRegion(RayTracingShaderGroupType::Callable),
        };

        vkCmdTraceRaysKHR(
            m_CommandBuffer,
            &regions[0],
            &regions[1],
            &regions[2],
            &regions[3],
            width,
            height,
            depth
        );
        m_AnyCommandRecorded = true;
    }

    void ComputeCommandEncoder::RebuildAccelerationStructureScene(Flourish::AccelerationStructure* accel, const AccelerationStructureSceneBuildInfo& buildInfo)
    {
        FL_CRASH_ASSERT(m_Encoding, "Cannot encode RebuildAccelerationStructureScene after encoding has ended");

        static_cast<AccelerationStructure*>(accel)->RebuildSceneInternal(buildInfo, m_CommandBuffer);
        m_AnyCommandRecorded = true;
    }

    void ComputeCommandEncoder::RebuildAccelerationStructureNode(Flourish::AccelerationStructure* accel, const AccelerationStructureNodeBuildInfo& buildInfo)
    {
        FL_CRASH_ASSERT(m_Encoding, "Cannot encode RebuildAccelerationStructureNode after encoding has ended");

        static_cast<AccelerationStructure*>(accel)->RebuildNodeInternal(buildInfo, m_CommandBuffer);
        m_AnyCommandRecorded = true;
    }
    
    void ComputeCommandEncoder::BindResourceSet(const Flourish::ResourceSet* set, u32 setIndex)
    {
        FL_CRASH_ASSERT(m_BoundComputePipeline || m_BoundRayTracingPipeline, "Must bind a pipeline before binding a resource set");

        m_DescriptorBinder.BindResourceSet(static_cast<const ResourceSet*>(set), setIndex);
    }

    void ComputeCommandEncoder::UpdateDynamicOffset(u32 setIndex, u32 bindingIndex, u32 offset)
    {
        FL_CRASH_ASSERT(m_BoundComputePipeline || m_BoundRayTracingPipeline, "Must bind a pipeline before updating dynamic offsets");

        m_DescriptorBinder.UpdateDynamicOffset(setIndex, bindingIndex, offset);
    }

    void ComputeCommandEncoder::FlushResourceSet(u32 setIndex)
    {
        FL_CRASH_ASSERT(m_BoundComputePipeline || m_BoundRayTracingPipeline, "Must bind a pipeline before flushing a resource set");

        VkPipelineLayout layout;
        VkPipelineBindPoint bind;
        if (m_BoundComputePipeline)
        {
            layout = m_BoundComputePipeline->GetLayout();
            bind = VK_PIPELINE_BIND_POINT_COMPUTE;
        }
        else
        {
            layout = m_BoundRayTracingPipeline->GetLayout();
            bind = VK_PIPELINE_BIND_POINT_RAY_TRACING_NV;
        }

        auto set = m_DescriptorBinder.GetResourceSet(setIndex);
        VkDescriptorSet sets[1] = { set->GetSet() };
        vkCmdBindDescriptorSets(
            m_CommandBuffer,
            bind,
            layout,
            setIndex, 1,
            sets,
            m_DescriptorBinder.GetDynamicOffsetCount(setIndex),
            m_DescriptorBinder.GetDynamicOffsetData(setIndex)
        );
    }

    void ComputeCommandEncoder::PushConstants(u32 offset, u32 size, const void* data)
    {
        FL_CRASH_ASSERT(m_BoundComputePipeline || m_BoundRayTracingPipeline, "Must bind a pipeline before pushing constants");
        FL_ASSERT(
            size <= m_DescriptorBinder.GetBoundData()->PushConstantRange.size,
            "Push constant size out of range"
        );

        VkPipelineLayout layout;
        if (m_BoundComputePipeline)
            layout = m_BoundComputePipeline->GetLayout();
        else
            layout = m_BoundRayTracingPipeline->GetLayout();
        
        vkCmdPushConstants(
            m_CommandBuffer,
            layout,
            m_DescriptorBinder.GetBoundData()->PushConstantRange.stageFlags,
            offset,
            size,
            data
        );
    }
}
