#pragma once

#include "Flourish/Api/Shader.h"
#include "Flourish/Backends/Vulkan/Util/Common.h"

namespace Flourish::Vulkan
{
    class DescriptorSetLayout
    {
    public:
        // TS
        void Initialize(const std::vector<ReflectionDataElement>& reflectionData);
        void Shutdown();

        // TS
        inline VkDescriptorSetLayout GetLayout() const { return m_Layout; }
        inline const auto& GetBindings() const { return m_Bindings; }
        inline auto& GetDynamicOffsets() { return m_DynamicOffsets; }
        inline auto& GetDescriptorWrites() { return m_CachedDescriptorWrites; }
        inline auto& GetDescriptorWrite(u32 bindingIndex) { return m_CachedDescriptorWrites[m_Bindings[bindingIndex].DescriptorWriteMapping]; }
        inline const auto& GetSetLayouts() { return m_CachedSetLayouts; }
        inline const auto& GetPoolSizes() { return m_CachedPoolSizes; }

        inline static constexpr u32 MaxSetsPerPool = 50;
        inline static constexpr u32 MaxUniqueDescriptors = 100;
        inline static constexpr u32 MaxDescriptorArrayCount = 20;

    private:
        struct BoundResource
        {
            void* Resource;
            u32 Offset;
            u32 Size;
        };
        struct BindingData
        {
            bool Exists = false;
            u32 DescriptorWriteMapping = 0;
            u32 OffsetIndex = 0;
        };

    private:
        VkDescriptorSetLayout m_Layout = nullptr;
        std::vector<BindingData> m_Bindings;
        std::vector<u32> m_DynamicOffsets;
        std::vector<VkWriteDescriptorSet> m_CachedDescriptorWrites;
        std::vector<VkDescriptorSetLayout> m_CachedSetLayouts;
        std::vector<VkDescriptorPoolSize> m_CachedPoolSizes;

        friend class DescriptorSet;
    };

    class DescriptorSet
    {
    public:
        DescriptorSet(const DescriptorSetLayout& layout);
        ~DescriptorSet();

        void UpdateBinding(u32 bindingIndex, ShaderResourceType resourceType, void* resource, bool useOffset, u32 offset, u32 size);
        void FlushBindings();

        // TS
        inline const DescriptorSetLayout& GetLayout() const { return m_Layout; }

    private:
        void CheckFrameUpdate();
        VkDescriptorPool CreateDescriptorPool();
        VkDescriptorSet AllocateSet();
        void ClearPools();
        u64 HashBindings();

    private:
        DescriptorSetLayout m_Layout;
        
        std::array<VkDescriptorBufferInfo, DescriptorSetLayout::MaxUniqueDescriptors> m_CachedBufferInfos;
        std::array<VkDescriptorImageInfo, DescriptorSetLayout::MaxDescriptorArrayCount * DescriptorSetLayout::MaxUniqueDescriptors> m_CachedImageInfos;
        std::array<std::unordered_map<u64, VkDescriptorSet>, Flourish::Context::MaxFrameBufferCount> m_CachedDescriptorSets;

        u64 m_LastResetFrame = 0;
        u32 m_WritesReadyCount = 0;
        std::unordered_map<u32, DescriptorSetLayout::BoundResource> m_BoundResources;
        std::array<std::vector<VkDescriptorPool>, Flourish::Context::MaxFrameBufferCount> m_DescriptorPools;
        std::array<std::vector<VkDescriptorSet>, Flourish::Context::MaxFrameBufferCount> m_AvailableSets;
        std::array<u32, Flourish::Context::MaxFrameBufferCount> m_AvailableSetIndex{};
        std::array<u32, Flourish::Context::MaxFrameBufferCount> m_AvailablePoolIndex{};
        VkDescriptorSet m_MostRecentDescriptorSet;

        std::mutex m_Mutex;
    };
}