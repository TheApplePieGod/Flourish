#include "flpch.h"
#include "DescriptorSet.h"

#include "Flourish/Backends/Vulkan/Context.h"

namespace Flourish::Vulkan
{
    DescriptorSet::DescriptorSet(
        const DescriptorSetCreateInfo& createInfo,
        const DescriptorSetAllocation& alloc,
        const std::shared_ptr<DescriptorPool>& parentPool
    )
        : Flourish::DescriptorSet(createInfo), m_ParentPool(parentPool), m_Allocation(alloc)
    {

        // TODO: Change name to resourcebindings or something more descriptive
        // Store descriptor pool inside shader
        // Mutex allocation
        //  - Don't need sync to update / use sets
        //  - Allocation should be extremely infrequent
        //  
        //  What do when buffer binds are dynamic? Should we have a dynamic
        //  vs static allocation option that will create a set for each frame
        //  each time binding???
    }

    void DescriptorSet::UpdateBinding(u32 bindingIndex, ShaderResourceType resourceType, void* resource, bool useOffset, u32 offset, u32 size)
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
                Buffer* buffer = static_cast<Buffer*>(resource);
                FL_ASSERT(bindingIndex < m_CachedBufferInfos.size(), "Binding index for buffer resource is too large");

                m_CachedBufferInfos[bufferInfoBaseIndex].buffer = buffer->GetBuffer();
                m_CachedBufferInfos[bufferInfoBaseIndex].offset = offset;
                m_CachedBufferInfos[bufferInfoBaseIndex].range = size;
            } break;

            case ShaderResourceType::Texture:
            case ShaderResourceType::StorageTexture:
            {
                Texture* texture = static_cast<Texture*>(resource);
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
                VkImageView view = static_cast<VkImageView>(resource);

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
}
