#pragma once

#include "Flourish/Api/ComputeCommandEncoder.h"
#include "Flourish/Backends/Vulkan/Util/Common.h"
#include "Flourish/Backends/Vulkan/Util/DescriptorSet.h"
#include "Flourish/Backends/Vulkan/Util/Commands.h"
#include "Flourish/Backends/Vulkan/ComputePipeline.h"

namespace Flourish::Vulkan
{
    class ComputeTarget;
    class CommandBuffer;
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
        
        void BindPipelineBufferResource(u32 bindingIndex, Flourish::Buffer* buffer, u32 bufferOffset, u32 dynamicOffset, u32 elementCount) override;
        void BindPipelineTextureResource(u32 bindingIndex, Flourish::Texture* texture) override;
        void BindPipelineTextureLayerResource(u32 bindingIndex, Flourish::Texture* texture, u32 layerIndex, u32 mipLevel) override;
        void FlushPipelineBindings() override;

        // TS
        inline VkCommandBuffer GetCommandBuffer() const { return m_CommandBuffer; }

    private:
        void ValidatePipelineBinding(u32 bindingIndex, ShaderResourceType resourceType, void* resource);

    private:
        bool m_FrameRestricted;
        CommandBufferAllocInfo m_AllocInfo;
        VkCommandBuffer m_CommandBuffer;
        CommandBuffer* m_ParentBuffer;
        ComputeTarget* m_BoundTarget = nullptr;
        DescriptorSet* m_BoundDescriptorSet = nullptr;
        ComputePipeline* m_BoundPipeline = nullptr;
    };
}