#include "flpch.h"
#include "DescriptorSet.h"

#include "Flourish/Backends/Vulkan/Context.h"
#include "Flourish/Backends/Vulkan/Buffer.h"
#include "Flourish/Backends/Vulkan/Texture.h"

namespace Flourish::Vulkan
{
    void DescriptorSetLayout::Initialize(const std::vector<ReflectionDataElement>& reflectionData)
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
            {
                bindingData.OffsetIndex = m_DynamicOffsets.size();
                m_DynamicOffsets.push_back(0);
            }

            m_Bindings.emplace_back(bindingData);
            lastBindingIndex = element.BindingIndex;
        }

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<u32>(bindings.size());
        layoutInfo.pBindings = bindings.data();

        if(!FL_VK_CHECK_RESULT(vkCreateDescriptorSetLayout(
            Context::Devices().Device(),
            &layoutInfo,
            nullptr,
            &m_Layout
        ), "DescriptorSet create layout"))
            throw std::exception();

        // Populate cached layouts because one is needed for each allocation
        for (u32 i = 0; i < MaxSetsPerPool; i++)
            m_CachedSetLayouts.emplace_back(m_Layout);

        // Populate the cached pool sizes
        for (auto& element : descriptorCounts)
        {
            VkDescriptorPoolSize poolSize{};
            poolSize.type = element.first;
            poolSize.descriptorCount = element.second;

            m_CachedPoolSizes.emplace_back(poolSize);
        }
    }

    void DescriptorSetLayout::Shutdown()
    {
        auto layout = m_Layout;
        Context::FinalizerQueue().Push([=]()
        {
            vkDestroyDescriptorSetLayout(Context::Devices().Device(), layout, nullptr);
        }, "Descriptor set layout free");
    }

    DescriptorSet::DescriptorSet(const DescriptorSetLayout& layout)
        : m_Layout(layout)
    {
        FL_ASSERT(m_Layout.GetLayout(), "Initializing descriptor set with uninitalized layout");

        for (u32 i = 0; i < Flourish::Context::FrameBufferCount(); i++)
            m_AvailableSets[i].resize(DescriptorSetLayout::MaxSetsPerPool);

        // Create the initial descriptor pools per frame
        for (size_t i = 0; i < m_DescriptorPools.size(); i++)
            m_DescriptorPools[i].emplace_back(CreateDescriptorPool());
    }

    DescriptorSet::~DescriptorSet()
    {
        auto pools = m_DescriptorPools;
        Context::FinalizerQueue().Push([=]()
        {
            for (u32 i = 0; i < pools.size(); i++)
                for (auto pool : pools[i])
                    vkDestroyDescriptorPool(Context::Devices().Device(), pool, nullptr);
        }, "Descriptor set free");
    }

    void DescriptorSet::UpdateBinding(u32 bindingIndex, ShaderResourceType resourceType, const void* resource, bool useOffset, u32 offset, u32 size)
    {
        CheckFrameUpdate();

        auto& boundResource = m_BoundResources[bindingIndex];
        if (boundResource.Resource == resource && boundResource.Offset == offset && boundResource.Size == size) // Don't do anything if this resource is already bound
            return;
        boundResource.Resource = resource;
        boundResource.Offset = offset;
        boundResource.Size = size;

        if (bindingIndex >= m_Layout.GetBindings().size() || !m_Layout.GetBindings()[bindingIndex].Exists)
        {
            FL_LOG_ERROR("Attempting to update descriptor binding %d that doesn't exist in the shader", bindingIndex);
            throw std::exception();
        }

        u32 bufferInfoBaseIndex = bindingIndex;
        u32 imageInfoBaseIndex = bindingIndex * DescriptorSetLayout::MaxDescriptorArrayCount;
        switch (resourceType)
        {
            default: { FL_ASSERT(false, "Cannot update descriptor set with selected resource type"); } break;

            case ShaderResourceType::UniformBuffer:
            case ShaderResourceType::StorageBuffer:
            {
                const Buffer* buffer = static_cast<const Buffer*>(resource);
                FL_ASSERT(bindingIndex < m_CachedBufferInfos.size(), "Binding index for buffer resource is too large");

                m_CachedBufferInfos[bufferInfoBaseIndex].buffer = buffer->GetBuffer();
                m_CachedBufferInfos[bufferInfoBaseIndex].offset = offset;
                m_CachedBufferInfos[bufferInfoBaseIndex].range = size;
            } break;

            case ShaderResourceType::Texture:
            case ShaderResourceType::StorageTexture:
            {
                const Texture* texture = static_cast<const Texture*>(resource);
                FL_ASSERT(texture->GetArrayCount() <= DescriptorSetLayout::MaxDescriptorArrayCount, "Image array count too large");

                for (u32 i = 0; i < texture->GetArrayCount(); i++)
                {
                    m_CachedImageInfos[imageInfoBaseIndex + i].sampler = texture->GetSampler();
                    m_CachedImageInfos[imageInfoBaseIndex + i].imageLayout = resourceType == ShaderResourceType::Texture ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_GENERAL;
                    if (useOffset)
                        m_CachedImageInfos[imageInfoBaseIndex + i].imageView = texture->GetLayerImageView(offset, size); // size is the mip level here
                    else
                        m_CachedImageInfos[imageInfoBaseIndex + i].imageView = texture->GetImageView();
                }
            } break;

            case ShaderResourceType::SubpassInput:
            {
                FL_ASSERT(bindingIndex < m_CachedImageInfos.size(), "Binding index for subpass input is too large");
                VkImageView view = (VkImageView)resource;

                // useOffset: is the attachment a color attachment
                m_CachedImageInfos[imageInfoBaseIndex].sampler = NULL;
                m_CachedImageInfos[imageInfoBaseIndex].imageLayout = useOffset ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL;
                m_CachedImageInfos[imageInfoBaseIndex].imageView = view;
            } break;
        }

        VkWriteDescriptorSet& descriptorWrite = m_Layout.GetDescriptorWrite(bindingIndex);
        if (descriptorWrite.pBufferInfo == nullptr && descriptorWrite.pImageInfo == nullptr)
            m_WritesReadyCount++;

        descriptorWrite.pBufferInfo = &m_CachedBufferInfos[bufferInfoBaseIndex];
        descriptorWrite.pImageInfo = &m_CachedImageInfos[imageInfoBaseIndex];
    }

    void DescriptorSet::FlushBindings()
    {
        CheckFrameUpdate();

        FL_ASSERT(m_WritesReadyCount == m_Layout.GetDescriptorWrites().size(), "Cannot flush bindings until all binding slots have been bound");

        //u64 hash = HashBindings();
        u32 frameIndex = Flourish::Context::FrameIndex();
        /*
        if (m_CachedDescriptorSets[frameIndex].find(hash) != m_CachedDescriptorSets[frameIndex].end())
        {
            m_MostRecentDescriptorSet = m_CachedDescriptorSets[frameIndex][hash];
            return;
        }
        */

        m_MostRecentDescriptorSet = AllocateSet();
        for (auto& write : m_Layout.GetDescriptorWrites())
            write.dstSet = m_MostRecentDescriptorSet;
        
        vkUpdateDescriptorSets(
            Context::Devices().Device(),
            static_cast<u32>(m_Layout.GetDescriptorWrites().size()),
            m_Layout.GetDescriptorWrites().data(),
            0, nullptr
        );

        //m_CachedDescriptorSets[frameIndex][hash] = m_MostRecentDescriptorSet;
    }

    void DescriptorSet::CheckFrameUpdate()
    {
        // Each frame, the bound resources need to be cleared and rebound
        if (Flourish::Context::FrameCount() != m_LastResetFrame)
        {
            m_LastResetFrame = Flourish::Context::FrameCount();
            m_BoundResources.clear();
            ClearPools();
            
            m_WritesReadyCount = 0;
            for (auto& write : m_Layout.GetDescriptorWrites())
            {
                write.pBufferInfo = nullptr;
                write.pImageInfo = nullptr;
            }
        }
    }

    VkDescriptorPool DescriptorSet::CreateDescriptorPool()
    {
        if (m_Layout.GetPoolSizes().empty()) return nullptr; // No descriptors so don't create pool

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = static_cast<u32>(m_Layout.GetPoolSizes().size());
        poolInfo.pPoolSizes = m_Layout.GetPoolSizes().data();
        poolInfo.maxSets = DescriptorSetLayout::MaxSetsPerPool;
        poolInfo.flags = 0;

        VkDescriptorPool pool;
        FL_VK_ENSURE_RESULT(vkCreateDescriptorPool(
            Context::Devices().Device(),
            &poolInfo,
            nullptr,
            &pool
        ), "DescriptorSet create pool");

        return pool;
    }
    
    VkDescriptorSet DescriptorSet::AllocateSet()
    {
        // Generate if we go over the size limit or if this is the first allocation
        u32 frameIndex = Flourish::Context::FrameIndex();
        if (m_AvailablePoolIndex[frameIndex] == 0 || m_AvailableSetIndex[frameIndex] >= m_AvailableSets[frameIndex].size())
        {
            m_AvailableSetIndex[frameIndex] = 0;

            if (m_AvailablePoolIndex[frameIndex] >= m_DescriptorPools[frameIndex].size())
                m_DescriptorPools[frameIndex].push_back(CreateDescriptorPool());

            VkDescriptorSetAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            allocInfo.descriptorPool = m_DescriptorPools[frameIndex][m_AvailablePoolIndex[frameIndex]++];
            allocInfo.descriptorSetCount = DescriptorSetLayout::MaxSetsPerPool;
            allocInfo.pSetLayouts = m_Layout.GetSetLayouts().data();

            VkResult result = vkAllocateDescriptorSets(Context::Devices().Device(), &allocInfo, m_AvailableSets[frameIndex].data());
        }
        return m_AvailableSets[frameIndex][m_AvailableSetIndex[frameIndex]++];
    }

    void DescriptorSet::ClearPools()
    {
        u32 frameIndex = Flourish::Context::FrameIndex();
        m_AvailableSetIndex[frameIndex] = 0;
        m_AvailablePoolIndex[frameIndex] = 0;
        m_CachedDescriptorSets[frameIndex].clear();

        for (auto& pool : m_DescriptorPools[frameIndex])
            vkResetDescriptorPool(Context::Devices().Device(), pool, 0);
    }

    u64 DescriptorSet::HashBindings()
    {
        u64 hash = 78316527u;

        for (auto& pair : m_BoundResources)
        {
            // Cantor pairing
            const u32 first = pair.first;
            const uptr second = (uptr)pair.second.Resource;
            const u32 third = pair.second.Offset;
            const u32 fourth = pair.second.Size;
            hash = (hash + first) * (hash + first + 1) / 2 + first;
            hash = (hash + second) * (hash + second + 1) / 2 + second;
            hash = (hash + third) * (hash + third + 1) / 2 + third;
            hash = (hash + fourth) * (hash + fourth + 1) / 2 + fourth;
        }

        return hash;
    }
}