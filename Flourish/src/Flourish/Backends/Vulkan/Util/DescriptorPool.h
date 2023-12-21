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

    struct ReflectionDataElement;
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
        bool CheckCompatibility(const DescriptorPool* other) const;

        // TS
        inline VkDescriptorSetLayout GetLayout() const { return m_Layout; }
        inline bool HasDescriptors() const { return !m_CachedPoolSizes.empty(); }
        inline u32 GetBufferCount() const { return m_BufferCount; }
        inline u32 GetAccelStructCount() const { return m_AccelStructCount; }
        inline u32 GetImageArrayElementCount() const { return m_ImageArrayElements; }
        inline const auto& GetBindingData() const { return m_Bindings; }

        // TS
        inline bool DoesBindingExist(u32 bindingIndex) const
        {
            return bindingIndex < m_Bindings.size() && m_Bindings[bindingIndex].Exists;
        }
        inline ShaderResourceType GetBindingType(u32 bindingIndex) const
        {
            return Common::RevertShaderResourceType(m_Bindings[bindingIndex].Type);
        }

        inline static constexpr u32 MaxSetsPerPool = 20;

    private:
        struct BindingData
        {
            bool Exists = false;
            u32 ArrayCount;
            VkDescriptorType Type;
            VkShaderStageFlags Access;
            u32 BufferArrayIndex = 0;
            u32 AccelArrayIndex = 0;
            u32 ImageArrayIndex = 0;
        };

        struct PoolData
        {
            VkDescriptorPool Pool = VK_NULL_HANDLE;
            u32 AllocatedCount = 0;
        };

    private:
        void CreateDescriptorPool();

    private:
        VkDescriptorSetLayout m_Layout = VK_NULL_HANDLE;

        std::vector<PoolData> m_DescriptorPools;
        std::vector<u32> m_AvailablePools;
        std::mutex m_PoolsMutex;
        
        std::vector<BindingData> m_Bindings;
        std::vector<VkDescriptorPoolSize> m_CachedPoolSizes;
        u32 m_BufferCount = 0;
        u32 m_AccelStructCount = 0;
        u32 m_ImageArrayElements = 0;
    };
}
