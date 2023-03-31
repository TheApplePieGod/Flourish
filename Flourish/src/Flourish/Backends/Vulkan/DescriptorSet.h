#pragma once

#include "Flourish/Api/DescriptorSet.h"
#include "Flourish/Backends/Vulkan/Util/Common.h"
#include "Flourish/Backends/Vulkan/Util/DescriptorPool.h"

namespace Flourish::Vulkan
{
    class DescriptorSet : public Flourish::DescriptorSet 
    {
    public:
        DescriptorSet(
            const DescriptorSetCreateInfo& createInfo,
            const std::shared_ptr<DescriptorPool>& parentPool
        );
        ~DescriptorSet() override;

        void UpdateDynamicOffset(u32 bindingIndex, u32 offset) override;
        void BindBuffer(u32 bindingIndex, const Flourish::Buffer* buffer, u32 bufferOffset, u32 elementCount) override;
        void BindTexture(u32 bindingIndex, const Flourish::Texture* texture) override;
        void BindTextureLayer(u32 bindingIndex, const Flourish::Texture* texture, u32 layerIndex, u32 mipLevel) override;
        void BindSubpassInput(u32 bindingIndex, SubpassAttachment attachment) override;
        void FlushBindings() override;

        // TS
        VkDescriptorSet GetSet() const;

    private:
        struct CachedData
        {
            u32 WritesReadyCount = 0;
            std::vector<VkWriteDescriptorSet> DescriptorWrites;
            std::array<VkDescriptorBufferInfo, DescriptorPool::MaxUniqueDescriptors> BufferInfos;
            std::array<VkDescriptorImageInfo, DescriptorPool::MaxDescriptorArrayCount * DescriptorPool::MaxUniqueDescriptors> ImageInfos;
        };

    private:
        void ValidateBinding(u32 bindingIndex, ShaderResourceType resourceType, const void* resource);
        void UpdateBinding(
            u32 bindingIndex,
            ShaderResourceType resourceType,
            const void* resource,
            bool useOffset,
            u32 offset,
            u32 size
        );

    private:
        u64 m_LastFrameWrite = 0;
        u32 m_AllocationCount = 1;

        std::vector<u32> m_DynamicOffsets;
        std::vector<CachedData> m_CachedData;

        std::shared_ptr<DescriptorPool> m_ParentPool;
        std::array<DescriptorSetAllocation, Flourish::Context::MaxFrameBufferCount> m_Allocations;
    };
}
