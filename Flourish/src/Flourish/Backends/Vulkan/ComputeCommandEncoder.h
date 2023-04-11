#pragma once

#include "Flourish/Api/ComputeCommandEncoder.h"
#include "Flourish/Backends/Vulkan/Util/Common.h"
#include "Flourish/Backends/Vulkan/Util/Commands.h"
#include "Flourish/Backends/Vulkan/ComputePipeline.h"
#include "Flourish/Backends/Vulkan/Util/DescriptorBinder.h"

namespace Flourish::Vulkan
{
    class ComputeTarget;
    class CommandBuffer;
    class DescriptorSet;
    class ComputeCommandEncoder : public Flourish::ComputeCommandEncoder 
    {
    public:
        ComputeCommandEncoder() = default;
        ComputeCommandEncoder(CommandBuffer* parentBuffer, bool frameRestricted);

        void BeginEncoding(ComputeTarget* target);
        void EndEncoding() override;
        void BindPipeline(Flourish::ComputePipeline* pipeline) override;
        void Dispatch(u32 x, u32 y, u32 z) override;
        void DispatchIndirect(Flourish::Buffer* buffer, u32 commandOffset) override;
        
        void BindDescriptorSet(const Flourish::DescriptorSet* set, u32 setIndex) override;
        void UpdateDynamicOffset(u32 setIndex, u32 bindingIndex, u32 offset) override;
        void FlushDescriptorSet(u32 setIndex) override;

        // TS
        inline VkCommandBuffer GetCommandBuffer() const { return m_CommandBuffer; }

    private:
        bool m_FrameRestricted;
        CommandBufferAllocInfo m_AllocInfo;
        VkCommandBuffer m_CommandBuffer;
        CommandBuffer* m_ParentBuffer;
        ComputeTarget* m_BoundTarget = nullptr;
        ComputePipeline* m_BoundPipeline = nullptr;
        DescriptorBinder m_DescriptorBinder;
    };
}
