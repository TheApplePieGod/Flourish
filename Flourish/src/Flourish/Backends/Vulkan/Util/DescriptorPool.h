#pragma once

#include "Flourish/Api/Shader.h"
#include "Flourish/Backends/Vulkan/Util/Common.h"

namespace Flourish::Vulkan
{
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
        bool IsResourceCorrectType(u32 bindingIndex, ShaderResourceType resourceType) const;

        // TS
        inline VkDescriptorSetLayout GetLayout() const { return m_Layout; }
        inline bool HasDescriptors() const { return !m_CachedPoolSizes.empty(); }
        inline u32 GetBufferCount() const { return m_BufferCount; }
        inline u32 GetImageArrayElementCount() const { return m_ImageArrayElements; }
        inline const auto& GetBindingData() const { return m_Bindings; }
        inline const auto& GetCachedWrites() const { return m_CachedDescriptorWrites; }

        // TS
        inline bool DoesBindingExist(u32 bindingIndex) const
        {
            return bindingIndex < m_Bindings.size() && m_Bindings[bindingIndex].Exists;
        }
        inline ShaderResourceType GetBindingType(u32 bindingIndex) const
        {
            return Common::RevertShaderResourceType(m_CachedDescriptorWrites[m_Bindings[bindingIndex].DescriptorWriteMapping].descriptorType);
        }

        inline static constexpr u32 MaxSetsPerPool = 50;

    private:
        struct BindingData
        {
            bool Exists = false;
            u32 DescriptorWriteMapping = 0;
            u32 BufferArrayIndex = 0;
            u32 ImageArrayIndex = 0;
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
        u32 m_BufferCount = 0;
        u32 m_ImageArrayElements = 0;
    };
}
