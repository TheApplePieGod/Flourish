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

        void BindBuffer(u32 bindingIndex, const Flourish::Buffer* buffer, u32 bufferOffset, u32 elementCount) override;
        void BindTexture(u32 bindingIndex, const Flourish::Texture* texture) override;
        void BindTextureLayer(u32 bindingIndex, const Flourish::Texture* texture, u32 layerIndex, u32 mipLevel) override;
        void BindSubpassInput(u32 bindingIndex, const Flourish::Framebuffer* framebuffer, SubpassAttachment attachment) override;
        void FlushBindings() override;

        // TS
        VkDescriptorSet GetSet() const;
        
        // TS
        inline const DescriptorPool* GetParentPool() const { return m_ParentPool.get(); }

    private:
        struct CachedData
        {
            u32 WritesReadyCount = 0;
            std::vector<VkWriteDescriptorSet> DescriptorWrites;
            std::vector<VkDescriptorBufferInfo> BufferInfos;
            std::vector<VkDescriptorImageInfo> ImageInfos;
        };

        struct SetList
        {
            u32 FreeIndex = 0;
            std::vector<DescriptorSetAllocation> Sets;
        };

    private:
        void SwapNextAllocation();
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

        std::vector<CachedData> m_CachedData;

        std::shared_ptr<DescriptorPool> m_ParentPool;
        std::array<DescriptorSetAllocation, Flourish::Context::MaxFrameBufferCount> m_Allocations;
        std::array<SetList, Flourish::Context::MaxFrameBufferCount> m_SetLists;
    };
}
