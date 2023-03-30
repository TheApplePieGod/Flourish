#pragma once

#include "Flourish/Api/Shader.h"
#include "Flourish/Backends/Vulkan/Util/Common.h"

namespace Flourish::Vulkan
{
    class DescriptorSetLayout
    {
    public:
        void Initialize(const std::vector<ReflectionDataElement>& reflectionData);
        void Shutdown();

        // TS
        inline VkDescriptorSetLayout GetLayout() const { return m_Layout; }
        inline const auto& GetBindings() const { return m_Bindings; }
        inline const auto& GetDynamicOffsets() const { return m_DynamicOffsets; }
        inline auto& GetDescriptorWrites() { return m_CachedDescriptorWrites; }
        inline auto& GetDescriptorWrite(u32 bindingIndex) { return m_CachedDescriptorWrites[m_Bindings[bindingIndex].DescriptorWriteMapping]; }
        inline const auto& GetSetLayouts() { return m_CachedSetLayouts; }
        inline const auto& GetPoolSizes() { return m_CachedPoolSizes; }
        
        // TS
        inline bool DoesBindingExist(u32 bindingIndex) const
        {
            return bindingIndex < m_Bindings.size() && m_Bindings[bindingIndex].Exists;
        }
        inline ShaderResourceType GetBindingType(u32 bindingIndex) const
        {
            return Common::RevertShaderResourceType(m_CachedDescriptorWrites[m_Bindings[bindingIndex].DescriptorWriteMapping].descriptorType);
        }
        bool IsResourceCorrectType(u32 bindingIndex, ShaderResourceType resourceType) const
        {
            auto actualType = m_CachedDescriptorWrites[m_Bindings[bindingIndex].DescriptorWriteMapping].descriptorType;
            return (
                Common::ConvertShaderResourceType(resourceType) == actualType ||
                (resourceType == ShaderResourceType::Texture && actualType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE) ||
                (resourceType == ShaderResourceType::StorageTexture && actualType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
            );
        }
    };

    struct DescriptorSetAllocation
    {
        VkDescriptorSet Set;
        u32 PoolIndex;
    };

    class DescriptorPool
    {
    public:
        DescriptorPool(const std::vector<ReflectionDataElement>& reflectionData);
        ~DescriptorPool();

        // TS
        DescriptorSetAllocation AllocateSet();
        void FreeSet(const DescriptorSetAllocation& allocation);

        // TS
        inline VkDescriptorSetLayout GetLayout() const { return m_Layout; }
        inline bool HasDescriptors() const { return !m_CachedPoolSizes.empty(); }
        inline const auto& GetBindingData() const { return m_Bindings; }

        inline static constexpr u32 MaxSetsPerPool = 50;
        inline static constexpr u32 MaxUniqueDescriptors = 100;
        inline static constexpr u32 MaxDescriptorArrayCount = 20;

    private:
        struct BindingData
        {
            bool Exists = false;
            u32 DescriptorWriteMapping = 0;
            u32 OffsetIndex = 0;
        };

        struct PoolData
        {
            VkDescriptorPool Pool = nullptr;
            u32 AllocatedCount = 0;
        };

    private:
        void CreateDescriptorPool();

    private:
        VkDescriptorSetLayout m_Layout = nullptr;

        std::vector<PoolData> m_DescriptorPools;
        std::vector<u32> m_AvailablePools;
        std::mutex m_PoolsMutex;
        
        std::vector<BindingData> m_Bindings;
        std::vector<VkWriteDescriptorSet> m_CachedDescriptorWrites;
        std::vector<VkDescriptorPoolSize> m_CachedPoolSizes;
        u32 m_DynamicOffsetsCount = 0;
    };
}
