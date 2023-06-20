#pragma once

#include "Flourish/Api/ComputeCommandEncoder.h"
#include "Flourish/Backends/Vulkan/Util/Common.h"
#include "Flourish/Backends/Vulkan/Util/Commands.h"
#include "Flourish/Backends/Vulkan/ComputePipeline.h"
#include "Flourish/Backends/Vulkan/Util/DescriptorBinder.h"

namespace Flourish::Vulkan
{
    class CommandBuffer;
    class ResourceSet;
    class ComputeCommandEncoder : public Flourish::ComputeCommandEncoder 
    {
    public:
        ComputeCommandEncoder() = default;
        ComputeCommandEncoder(CommandBuffer* parentBuffer, bool frameRestricted);

        void BeginEncoding();
        void EndEncoding() override;
        void BindPipeline(Flourish::ComputePipeline* pipeline) override;
        void Dispatch(u32 x, u32 y, u32 z) override;
        void DispatchIndirect(Flourish::Buffer* buffer, u32 commandOffset) override;
        void RebuildAccelerationStructureScene(AccelerationStructure* accel, const AccelerationStructureSceneBuildInfo& buildInfo) override;
        void RebuildAccelerationStructureNode(AccelerationStructure* accel, const AccelerationStructureNodeBuildInfo& buildInfo) override;
        
        void BindResourceSet(const Flourish::ResourceSet* set, u32 setIndex) override;
        void UpdateDynamicOffset(u32 setIndex, u32 bindingIndex, u32 offset) override;
        void FlushResourceSet(u32 setIndex) override;

        // TS
        inline VkCommandBuffer GetCommandBuffer() const { return m_CommandBuffer; }
        inline void MarkManuallyRecorded() { m_AnyCommandRecorded = true; }

    private:
        bool m_AnyCommandRecorded = false;
        bool m_FrameRestricted;
        VkCommandBuffer m_CommandBuffer;
        CommandBufferEncoderSubmission m_Submission;
        CommandBuffer* m_ParentBuffer;
        ComputePipeline* m_BoundPipeline = nullptr;
        DescriptorBinder m_DescriptorBinder;
    };
}
