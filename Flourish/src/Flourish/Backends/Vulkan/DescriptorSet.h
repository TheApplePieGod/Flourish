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
            const DescriptorSetAllocation& alloc,
            const std::shared_ptr<DescriptorPool>& parentPool
        );
        ~DescriptorSet() override;

        void BindBuffer(u32 bindingIndex, Flourish::Buffer* buffer, u32 bufferOffset, u32 dynamicOffset, u32 elementCount) override;
        void BindTexture(u32 bindingIndex, Flourish::Texture* texture) override;
        void BindTextureLayer(u32 bindingIndex, Flourish::Texture* texture, u32 layerIndex, u32 mipLevel) override;
        void BindSubpassInput(u32 bindingIndex, SubpassAttachment attachment) override;
        void FlushBindings() override;

    private:
        std::array<VkDescriptorBufferInfo, MaxUniqueDescriptors> m_CachedBufferInfos;
        std::array<VkDescriptorImageInfo, MaxDescriptorArrayCount * MaxUniqueDescriptors> m_CachedImageInfos;

        std::shared_ptr<DescriptorPool> m_ParentPool;
        DescriptorSetAllocation m_Allocation;
    };
}
