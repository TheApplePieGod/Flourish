#pragma once

#include "Flourish/Api/ResourceSet.h"
#include "Flourish/Backends/Vulkan/Util/Common.h"
#include "Flourish/Backends/Vulkan/Util/DescriptorPool.h"

namespace Flourish::Vulkan
{
    class ResourceSet : public Flourish::ResourceSet 
    {
    public:
        ResourceSet(
            const ResourceSetCreateInfo& createInfo,
            ResourceSetPipelineCompatability compatability,
            const std::shared_ptr<DescriptorPool>& parentPool
        );
        ~ResourceSet() override;

        void BindBuffer(u32 bindingIndex, const std::shared_ptr<Flourish::Buffer>& buffer, u32 bufferOffset, u32 elementCount) override;
        void BindTexture(u32 bindingIndex, const std::shared_ptr<Flourish::Texture>& texture, u32 arrayIndex = 0) override;
        void BindTextureLayer(u32 bindingIndex, const std::shared_ptr<Flourish::Texture>& texture, u32 layerIndex, u32 mipLevel, u32 arrayIndex = 0) override;
        void BindSubpassInput(u32 bindingIndex, const std::shared_ptr<Flourish::Framebuffer>& framebuffer, SubpassAttachment attachment) override;
        void BindAccelerationStructure(u32 bindingIndex, const std::shared_ptr<Flourish::AccelerationStructure>& accelStruct) override;
        void BindBuffer(u32 bindingIndex, const Flourish::Buffer* buffer, u32 bufferOffset, u32 elementCount) override;
        void BindTexture(u32 bindingIndex, const Flourish::Texture* texture, u32 arrayIndex = 0) override;
        void BindTextureLayer(u32 bindingIndex, const Flourish::Texture* texture, u32 layerIndex, u32 mipLevel, u32 arrayIndex = 0) override;
        void BindSubpassInput(u32 bindingIndex, const Flourish::Framebuffer* framebuffer, SubpassAttachment attachment) override;
        void BindAccelerationStructure(u32 bindingIndex, const Flourish::AccelerationStructure* accelStruct) override;
        void FlushBindings() override;

        // TS
        VkDescriptorSet GetSet() const;
        VkDescriptorSet GetSet(u32 frameIndex) const;
        
        // TS
        inline const DescriptorPool* GetParentPool() const { return m_ParentPool.get(); }

    private:
        struct StoredReferences
        {
            std::shared_ptr<Flourish::Buffer> Buffer;
            std::shared_ptr<Flourish::Texture> Texture;
            std::shared_ptr<Flourish::Framebuffer> Framebuffer;
            std::shared_ptr<Flourish::AccelerationStructure> AccelStruct;

            void Clear();
        };

        struct CachedData
        {
            std::vector<VkWriteDescriptorSet> DescriptorWrites;
            std::vector<VkDescriptorBufferInfo> BufferInfos;
            std::vector<VkDescriptorImageInfo> ImageInfos;
            std::vector<VkAccelerationStructureKHR> Accels;
            std::vector<VkWriteDescriptorSetAccelerationStructureKHR> AccelWrites;
        };

        struct SetList
        {
            u32 FreeIndex = 0;
            std::vector<DescriptorSetAllocation> Sets;
        };

    private:
        void SwapNextAllocation();
        bool ValidateBinding(
            u32 bindingIndex,
            ShaderResourceType resourceType,
            const void* resource,
            u32 arrayIndex
        );
        void UpdateBinding(
            u32 bindingIndex,
            ShaderResourceType resourceType,
            const void* resource,
            bool useOffset,
            u32 offset,
            u32 size,
            u32 arrayIndex
        );

    private:
        u64 m_LastFrameWrite = 0;
        u32 m_AllocationCount = 1;

        std::vector<CachedData> m_CachedData;
        std::vector<StoredReferences> m_StoredReferences;
        std::shared_ptr<DescriptorPool> m_ParentPool;
        std::array<DescriptorSetAllocation, Flourish::Context::MaxFrameBufferCount> m_Allocations;
        std::array<SetList, Flourish::Context::MaxFrameBufferCount> m_SetLists;
    };
}
