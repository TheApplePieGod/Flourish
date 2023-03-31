#include "flpch.h"
#include "DescriptorPool.h"

#include "Flourish/Backends/Vulkan/Context.h"
#include "Flourish/Backends/Vulkan/Buffer.h"
#include "Flourish/Backends/Vulkan/Texture.h"

namespace Flourish::Vulkan
{
    DescriptorPool::DescriptorPool(const std::vector<ReflectionDataElement>& reflectionData)
    {
        // Reflection data will give us sorted binding indexes so we can make some shortcuts here
        // Create the descriptor set layout and cache the associated pool sizes
        std::vector<VkDescriptorSetLayoutBinding> bindings;
        std::unordered_map<VkDescriptorType, u32> descriptorCounts;
        u32 lastBindingIndex = 0;
        for (auto& element : reflectionData)
        {
            BindingData bindingData{};
            bindingData.Exists = true;

            while (element.BindingIndex - lastBindingIndex > 1)
            {
                m_Bindings.emplace_back();
                lastBindingIndex++;
            }

            VkDescriptorSetLayoutBinding binding{};
            binding.binding = element.BindingIndex;
            binding.descriptorType = Common::ConvertShaderResourceType(element.ResourceType);
            binding.descriptorCount = element.ArrayCount;
            binding.stageFlags = Common::ConvertShaderResourceAccessType(element.AccessType);
            binding.pImmutableSamplers = nullptr;
            bindings.emplace_back(binding);

            descriptorCounts[binding.descriptorType] += element.ArrayCount * MaxSetsPerPool;

            // Populated the cached descriptor writes for updating new sets
            VkWriteDescriptorSet descriptorWrite{};
            descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrite.dstBinding = element.BindingIndex;
            descriptorWrite.dstArrayElement = 0;
            descriptorWrite.descriptorType = binding.descriptorType;
            descriptorWrite.descriptorCount = element.ArrayCount;

            bindingData.DescriptorWriteMapping = m_CachedDescriptorWrites.size(); 
            m_CachedDescriptorWrites.emplace_back(descriptorWrite);

            // Populate the dynamic offset info if applicable
            if (element.ResourceType == ShaderResourceType::UniformBuffer || element.ResourceType == ShaderResourceType::StorageBuffer)
                bindingData.OffsetIndex = m_DynamicOffsetsCount++;

            m_Bindings.emplace_back(bindingData);
            lastBindingIndex = element.BindingIndex;
        }

        // No descriptors
        if (descriptorCounts.empty())
            return;

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<u32>(bindings.size());
        layoutInfo.pBindings = bindings.data();

        if(!FL_VK_CHECK_RESULT(vkCreateDescriptorSetLayout(
            Context::Devices().Device(),
            &layoutInfo,
            nullptr,
            &m_Layout
        ), "DescriptorPool create layout"))
            throw std::exception();

        // Populate the cached pool sizes
        for (auto& element : descriptorCounts)
        {
            VkDescriptorPoolSize poolSize{};
            poolSize.type = element.first;
            poolSize.descriptorCount = element.second;

            m_CachedPoolSizes.emplace_back(poolSize);
        }
    }

    DescriptorPool::~DescriptorPool()
    {
        if (!HasDescriptors())
            return;

        auto layout = m_Layout;
        auto pools = m_DescriptorPools;
        Context::FinalizerQueue().Push([=]()
        {
            for (auto pool : pools)
                vkDestroyDescriptorPool(Context::Devices().Device(), pool.Pool, nullptr);
            vkDestroyDescriptorSetLayout(Context::Devices().Device(), layout, nullptr);
        }, "DescriptorPool free");
    }
    
    DescriptorSetAllocation DescriptorPool::AllocateSet()
    {
        FL_ASSERT(HasDescriptors(), "Cannot allocate set from pool with no descriptors");

        m_PoolsMutex.lock();

        if (m_AvailablePools.empty())
            CreateDescriptorPool();

        u32 poolIndex = m_AvailablePools.back();
        auto& pool = m_DescriptorPools[poolIndex];
        
        VkDescriptorSet set;
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = pool.Pool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &m_Layout;
        VkResult result = vkAllocateDescriptorSets(
            Context::Devices().Device(),
            &allocInfo,
            &set
        );

        if (++pool.AllocatedCount == MaxSetsPerPool)
            m_AvailablePools.pop_back();

        m_PoolsMutex.unlock();

        return { set, poolIndex };
    }

    void DescriptorPool::FreeSet(const DescriptorSetAllocation& allocation)
    {
        FL_ASSERT(HasDescriptors(), "Cannot free set from pool with no descriptors");

        m_PoolsMutex.lock();

        auto& pool = m_DescriptorPools[allocation.PoolIndex];
        if (pool.AllocatedCount-- == MaxSetsPerPool)
            m_AvailablePools.push_back(allocation.PoolIndex);

        vkFreeDescriptorSets(
            Context::Devices().Device(),
            pool.Pool,
            1, &allocation.Set
        );

        m_PoolsMutex.unlock();
    }

    bool DescriptorPool::IsResourceCorrectType(u32 bindingIndex, ShaderResourceType resourceType) const
    {
        // Some leniency when checking images
        // TODO: define this more concretely because it feels hacky
        auto actualType = m_CachedDescriptorWrites[m_Bindings[bindingIndex].DescriptorWriteMapping].descriptorType;
        return (
            Common::ConvertShaderResourceType(resourceType) == actualType ||
            (resourceType == ShaderResourceType::Texture && actualType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE) ||
            (resourceType == ShaderResourceType::StorageTexture && actualType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
        );
    }

    void DescriptorPool::CreateDescriptorPool()
    {
        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = static_cast<u32>(m_CachedPoolSizes.size());
        poolInfo.pPoolSizes = m_CachedPoolSizes.data();
        poolInfo.maxSets = MaxSetsPerPool;
        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

        VkDescriptorPool pool;
        FL_VK_ENSURE_RESULT(vkCreateDescriptorPool(
            Context::Devices().Device(),
            &poolInfo,
            nullptr,
            &pool
        ), "DescriptorPool create pool");

        m_AvailablePools.push_back(m_DescriptorPools.size());
        m_DescriptorPools.push_back({ pool });
    }
}
