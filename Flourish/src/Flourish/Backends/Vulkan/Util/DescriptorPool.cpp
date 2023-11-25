#include "flpch.h"
#include "DescriptorPool.h"

#include "Flourish/Backends/Vulkan/Context.h"
#include "Flourish/Backends/Vulkan/Buffer.h"
#include "Flourish/Backends/Vulkan/Texture.h"
#include "Flourish/Backends/Vulkan/Shader.h"

namespace Flourish::Vulkan
{
    DescriptorPool::DescriptorPool(const std::vector<ReflectionDataElement>& reflectionData)
    {
        // Reflection data will give us sorted binding indexes so we can make some shortcuts here
        // Create the descriptor set layout and cache the associated pool sizes
        std::vector<VkDescriptorSetLayoutBinding> bindings;
        std::vector<VkDescriptorBindingFlags> bindingFlags;
        std::unordered_map<VkDescriptorType, u32> descriptorCounts;
        u32 lastBindingIndex = 0;
        for (auto& element : reflectionData)
        {
            BindingData bindingData{};
            bindingData.Exists = true;

            FL_ASSERT(element.ArrayCount > 0, "Element must not have an unspecified array size");
            /*
            FL_ASSERT(
                element.ArrayCount > 0 || Flourish::Context::FeatureTable().BindlessShaderResources,
                "ArrayCount must not be zero unless BindlessShaderResources is enabled"
            );
            */

            while (element.BindingIndex - lastBindingIndex > 1)
            {
                m_Bindings.emplace_back();
                lastBindingIndex++;
            }

            VkDescriptorSetLayoutBinding binding{};
            binding.binding = element.BindingIndex;
            binding.descriptorType = Common::ConvertShaderResourceType(element.ResourceType);
            binding.descriptorCount = element.ArrayCount;
            binding.stageFlags = Common::ConvertShaderAccessType(element.AccessType);
            binding.pImmutableSamplers = nullptr;
            bindings.emplace_back(binding);

            bindingData.Type = binding.descriptorType;
            bindingData.ArrayCount = binding.descriptorCount;

            // TODO: this really is unnecessary on all bindings. We should have some
            // sort of preprocess in the shader that detects this intent
            VkDescriptorBindingFlags flags = 0;
            if (Flourish::Context::FeatureTable().PartiallyBoundResourceSets)
                flags |= VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
            if (element.ArrayCount == 0)
                flags |= VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;
            bindingFlags.emplace_back(flags);

            descriptorCounts[binding.descriptorType] += binding.descriptorCount * MaxSetsPerPool;

            // Populate cache information
            if (element.ResourceType == ShaderResourceType::AccelerationStructure)
                bindingData.AccelArrayIndex = m_AccelStructCount++;
            if (element.ResourceType == ShaderResourceType::UniformBuffer ||
                element.ResourceType == ShaderResourceType::StorageBuffer)
                bindingData.BufferArrayIndex = m_BufferCount++;
            else if (element.ResourceType == ShaderResourceType::Texture ||
                     element.ResourceType == ShaderResourceType::StorageTexture ||
                     element.ResourceType == ShaderResourceType::SubpassInput)
            {
                bindingData.ImageArrayIndex = m_ImageArrayElements;
                m_ImageArrayElements += element.ArrayCount;
            }

            m_Bindings.emplace_back(bindingData);
            lastBindingIndex = element.BindingIndex;
        }

        // No descriptors
        if (descriptorCounts.empty())
            return;

        VkDescriptorSetLayoutBindingFlagsCreateInfo flags{};
        flags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
        flags.bindingCount = bindings.size();
        flags.pBindingFlags = bindingFlags.data();

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        if (Flourish::Context::FeatureTable().PartiallyBoundResourceSets)
            layoutInfo.pNext = &flags;
        layoutInfo.bindingCount = static_cast<u32>(bindings.size());
        layoutInfo.pBindings = bindings.data();
        layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;

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
        auto actualType = m_Bindings[bindingIndex].Type;
        return (
            Common::ConvertShaderResourceType(resourceType) == actualType ||
            (resourceType == ShaderResourceType::Texture && actualType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE) ||
            (resourceType == ShaderResourceType::StorageTexture && actualType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
        );
    }

    // TODO: hashing? compatability table?
    // TODO: should also check accesstype compatability
    bool DescriptorPool::CheckCompatibility(const DescriptorPool* other) const
    {
        const auto& l = m_Bindings;
        const auto& r = other->m_Bindings;

        if (l.size() != r.size())
            return false;

        for (u32 i = 0; i < l.size(); i++)
        {
            if (l[i].Exists != r[i].Exists)
                return false;
            if (l[i].Type != r[i].Type ||
                l[i].ArrayCount != r[i].ArrayCount)
                return false;
        }

        return true;
    }

    void DescriptorPool::CreateDescriptorPool()
    {
        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = static_cast<u32>(m_CachedPoolSizes.size());
        poolInfo.pPoolSizes = m_CachedPoolSizes.data();
        poolInfo.maxSets = MaxSetsPerPool;
        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT |
                         VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;

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
