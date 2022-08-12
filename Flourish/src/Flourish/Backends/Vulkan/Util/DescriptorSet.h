#pragma once

#include "Flourish/Api/Shader.h"
#include "Flourish/Backends/Vulkan/Util/Common.h"

namespace Flourish::Vulkan
{
    class DescriptorSet
    {
    public:
        // TS
        void Initialize(const std::vector<ReflectionDataElement>& reflectionData);
        void Shutdown();
        void UpdateBinding(u32 bindingIndex, ShaderResourceType resourceType, void* resource, bool useOffset, u32 offset, u32 size);
        void FlushBindings();

        // TS
        inline VkDescriptorSetLayout GetLayout() const { return m_DescriptorSetLayout; };

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
        void CheckFrameUpdate();
        VkDescriptorPool CreateDescriptorPool();
        VkDescriptorSet AllocateSet();
        void ClearPools();
        u64 HashBindings();

    private:
        inline static constexpr u32 MaxSetsPerPool = 50;
        inline static constexpr u32 MaxUniqueDescriptors = 100;
        inline static constexpr u32 MaxDescriptorArrayCount = 20;

        VkDescriptorSetLayout m_DescriptorSetLayout;
        std::vector<VkDescriptorSetLayout> m_CachedSetLayouts;
        std::array<std::vector<VkDescriptorPool>, Flourish::Context::MaxFrameBufferCount> m_DescriptorPools;
        VkDescriptorSet m_MostRecentDescriptorSet;

        std::vector<BindingData> m_Bindings;
        std::unordered_map<u32, BoundResource> m_BoundResources;
        std::vector<u32> m_DynamicOffsets;
        std::vector<VkDescriptorPoolSize> m_CachedPoolSizes;
        std::vector<VkWriteDescriptorSet> m_CachedDescriptorWrites;
        std::array<VkDescriptorBufferInfo, MaxUniqueDescriptors> m_CachedBufferInfos;
        std::array<VkDescriptorImageInfo, MaxDescriptorArrayCount * MaxUniqueDescriptors> m_CachedImageInfos;
        std::array<std::unordered_map<u64, VkDescriptorSet>, Flourish::Context::MaxFrameBufferCount> m_CachedDescriptorSets;

        u64 m_LastResetFrame = 0;
        u32 m_WritesReadyCount = 0;
        std::array<std::vector<VkDescriptorSet>, Flourish::Context::MaxFrameBufferCount> m_AvailableSets;
        std::array<u32, Flourish::Context::MaxFrameBufferCount> m_AvailableSetIndex{};
        std::array<u32, Flourish::Context::MaxFrameBufferCount> m_AvailablePoolIndex{};

        std::mutex m_Mutex;
    };
}