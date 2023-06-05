#include "flpch.h"
#include "DescriptorSet.h"

#include "Flourish/Backends/Vulkan/Buffer.h"
#include "Flourish/Backends/Vulkan/Texture.h"
#include "Flourish/Backends/Vulkan/Framebuffer.h"
#include "Flourish/Backends/Vulkan/Context.h"

namespace Flourish::Vulkan
{
    DescriptorSet::DescriptorSet(
        const DescriptorSetCreateInfo& createInfo,
        const std::shared_ptr<DescriptorPool>& parentPool
    )
        : Flourish::DescriptorSet(createInfo), m_ParentPool(parentPool)
    {
        FL_ASSERT(
            static_cast<u8>(m_Info.Writability) & static_cast<u8>(DescriptorSetWritability::FrameWrite) ||
            !(static_cast<u8>(m_Info.Writability) & static_cast<u8>(DescriptorSetWritability::MultiWrite)),
            "Multiwrite cannot be enabled on a descriptor set if framewrite is not also enabled"
        );

        if (static_cast<u8>(m_Info.Writability) & static_cast<u8>(DescriptorSetWritability::DynamicData))
            m_AllocationCount = Flourish::Context::FrameBufferCount();
        for (u32 i = 0; i < m_AllocationCount; i++)
        {
            m_Allocations[i] = m_ParentPool->AllocateSet();

            m_CachedData.emplace_back();
            m_CachedData.back().DescriptorWrites = m_ParentPool->GetCachedWrites();
            for (auto& write : m_CachedData.back().DescriptorWrites)
                write.dstSet = m_Allocations[i].Set;
            m_CachedData.back().BufferInfos.resize(m_ParentPool->GetBufferCount());
            m_CachedData.back().ImageInfos.resize(m_ParentPool->GetImageArrayElementCount());

            // Ensure free lists are populated with initial allocations when using multiwrite
            if (static_cast<u8>(m_Info.Writability) & static_cast<u8>(DescriptorSetWritability::MultiWrite))
            {
                m_SetLists[i].Sets.push_back(m_Allocations[i]);
                m_SetLists[i].FreeIndex++;
            }
        }
    }

    DescriptorSet::~DescriptorSet()
    {
        auto allocs = m_Allocations;
        auto allocCount = m_AllocationCount;
        auto pool = m_ParentPool;
        Context::FinalizerQueue().Push([=]()
        {
            for (u32 i = 0; i < allocCount; i++)
                pool->FreeSet(allocs[i]);
        }, "DescriptorSet free");
    }

    void DescriptorSet::BindBuffer(u32 bindingIndex, const Flourish::Buffer* buffer, u32 bufferOffset, u32 elementCount)
    {
        FL_CRASH_ASSERT(elementCount + bufferOffset <= buffer->GetAllocatedCount(), "ElementCount + BufferOffset must be <= buffer allocated count");
        FL_CRASH_ASSERT(buffer->GetType() == BufferType::Uniform || buffer->GetType() == BufferType::Storage, "Buffer bind must be either a uniform or storage buffer");

        ShaderResourceType bufferType = buffer->GetType() == BufferType::Uniform ? ShaderResourceType::UniformBuffer : ShaderResourceType::StorageBuffer;
        ValidateBinding(bindingIndex, bufferType, buffer);

        if (!(static_cast<u8>(m_Info.Writability) & static_cast<u8>(DescriptorSetWritability::DynamicData)) && buffer->GetUsage() == BufferUsageType::Dynamic)
        {
            FL_LOG_ERROR("Cannot bind a dynamic buffer to a descriptor set with static writability");
            throw std::exception();
        }

        u32 stride = buffer->GetStride();
        UpdateBinding(
            bindingIndex, 
            bufferType, 
            buffer,
            true,
            stride * bufferOffset,
            stride * elementCount
        );
    }

    // TODO: ensure bound texture is not also being written to in framebuffer
    void DescriptorSet::BindTexture(u32 bindingIndex, const Flourish::Texture* texture)
    {
        ShaderResourceType texType =
            texture->GetUsageType() == TextureUsageType::ComputeTarget
                ? ShaderResourceType::StorageTexture
                : ShaderResourceType::Texture;
        
        ValidateBinding(bindingIndex, texType, texture);
        FL_ASSERT(
            m_ParentPool->GetBindingType(bindingIndex) != ShaderResourceType::StorageTexture || texType == ShaderResourceType::StorageTexture,
            "Attempting to bind a texture to a storage image binding that was not created as a compute target"
        );

        if (!(static_cast<u8>(m_Info.Writability) & static_cast<u8>(DescriptorSetWritability::DynamicData)) && texture->GetWritability() == TextureWritability::PerFrame)
        {
            FL_LOG_ERROR("Cannot bind a dynamic texture to a descriptor set with static writability");
            throw std::exception();
        }

        UpdateBinding(
            bindingIndex, 
            texType, 
            texture,
            false, 0, 0
        );
    }
    
    // TODO: ensure bound texture is not also being written to in framebuffer
    void DescriptorSet::BindTextureLayer(u32 bindingIndex, const Flourish::Texture* texture, u32 layerIndex, u32 mipLevel)
    {
        ShaderResourceType texType =
            texture->GetUsageType() == TextureUsageType::ComputeTarget
                ? ShaderResourceType::StorageTexture
                : ShaderResourceType::Texture;

        ValidateBinding(bindingIndex, texType, texture);
        FL_ASSERT(
            m_ParentPool->GetBindingType(bindingIndex) != ShaderResourceType::StorageTexture || texType == ShaderResourceType::StorageTexture,
            "Attempting to bind a texture to a storage image binding that was not created as a compute target"
        );

        UpdateBinding(
            bindingIndex, 
            texType, 
            texture,
            true, layerIndex, mipLevel
        );
    }

    // TODO: need to test this
    void DescriptorSet::BindSubpassInput(u32 bindingIndex, const Flourish::Framebuffer* framebuffer, SubpassAttachment attachment)
    {
        // This could be a tighter bound (i.e. also work with OnceDynamicData) if
        // we modified UpdateBinding slightly somehow
        if (!(static_cast<u8>(m_Info.Writability) & static_cast<u8>(DescriptorSetWritability::FrameWrite)))
        {
            FL_LOG_ERROR("Cannot bind a subpass input to a descriptor set without PerFrame writability");
            throw std::exception();
        }

        ValidateBinding(bindingIndex, ShaderResourceType::SubpassInput, &attachment);
        
        VkImageView attView = static_cast<const Framebuffer*>(framebuffer)->GetAttachmentImageView(attachment);

        UpdateBinding(
            bindingIndex, 
            ShaderResourceType::SubpassInput, 
            attView,
            attachment.Type == SubpassAttachmentType::Color, 0, 0
        );
    }

    void DescriptorSet::SwapNextAllocation()
    {
        auto& list = m_SetLists[Flourish::Context::FrameIndex()];

        // New frame, reset free list pointer
        if (m_LastFrameWrite != Flourish::Context::FrameCount())
            list.FreeIndex = 0;

        // Update allocation
        if (list.FreeIndex >= list.Sets.size())
            list.Sets.push_back(m_ParentPool->AllocateSet());
        m_Allocations[Flourish::Context::FrameIndex()] = list.Sets[list.FreeIndex++];
        for (auto& write : m_CachedData[Flourish::Context::FrameIndex()].DescriptorWrites)
            write.dstSet = m_Allocations[Flourish::Context::FrameIndex()].Set;
    }

    // TODO: might want to disable this in release?
    void DescriptorSet::ValidateBinding(u32 bindingIndex, ShaderResourceType resourceType, const void* resource)
    {
        if (resource == nullptr)
        {
            FL_LOG_ERROR("Cannot bind a null resource to a descriptor set");
            throw std::exception();
        }

        if (!m_ParentPool->DoesBindingExist(bindingIndex))
        {
            FL_LOG_ERROR("Attempting to update descriptor binding %d that doesn't exist in the set", bindingIndex);
            throw std::exception();
        }

        if (!m_ParentPool->IsResourceCorrectType(bindingIndex, resourceType))
        {
            FL_LOG_ERROR("Attempting to bind a resource that does not match the bind index type");
            throw std::exception();
        }
    }

    void DescriptorSet::UpdateBinding(
        u32 bindingIndex,
        ShaderResourceType resourceType,
        const void* resource,
        bool useOffset,
        u32 offset,
        u32 size
    )
    {
        // Update descriptor information. The for loop is structured such that the
        // following behavior occurs for different writabilities:
        //     - OnceStaticData: one cache entry is written, frame index won't matter
        //                       since we have already validated that resource is static
        //     - OnceDynamicData: cache entry is written for each frameindex
        //     - PerFrame: cache entry for current frameindex is written
        u32 bufferInfoBaseIndex = m_ParentPool->GetBindingData()[bindingIndex].BufferArrayIndex;
        u32 imageInfoBaseIndex = m_ParentPool->GetBindingData()[bindingIndex].ImageArrayIndex;
        u32 frameIndex = Flourish::Context::FrameIndex();
        for (u32 i = 0; i < m_AllocationCount; i++)
        {
            auto& cachedData = m_AllocationCount == 1 ? m_CachedData[0] : m_CachedData[frameIndex];
            auto& bufferInfos = cachedData.BufferInfos;
            auto& imageInfos = cachedData.ImageInfos;
            switch (resourceType)
            {
                default: { FL_ASSERT(false, "Cannot update descriptor set with selected resource type"); } break;

                case ShaderResourceType::UniformBuffer:
                case ShaderResourceType::StorageBuffer:
                {
                    const Buffer* buffer = static_cast<const Buffer*>(resource);

                    bufferInfos[bufferInfoBaseIndex].buffer = buffer->GetBuffer(frameIndex);
                    bufferInfos[bufferInfoBaseIndex].offset = offset;
                    bufferInfos[bufferInfoBaseIndex].range = size;
                } break;

                case ShaderResourceType::Texture:
                case ShaderResourceType::StorageTexture:
                {
                    const Texture* texture = static_cast<const Texture*>(resource);

                    for (u32 i = 0; i < texture->GetArrayCount(); i++)
                    {
                        imageInfos[imageInfoBaseIndex + i].sampler = texture->GetSampler();
                        imageInfos[imageInfoBaseIndex + i].imageLayout = resourceType == ShaderResourceType::Texture ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_GENERAL;
                        if (useOffset)
                            // size is the mip level here
                            imageInfos[imageInfoBaseIndex + i].imageView = texture->GetLayerImageView(frameIndex, offset, size); 
                        else
                            imageInfos[imageInfoBaseIndex + i].imageView = texture->GetImageView(frameIndex);
                    }
                } break;

                case ShaderResourceType::SubpassInput:
                {
                    VkImageView view = (VkImageView)resource;

                    // useOffset: is the attachment a color attachment
                    imageInfos[imageInfoBaseIndex].sampler = NULL;
                    imageInfos[imageInfoBaseIndex].imageLayout = useOffset ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL;
                    imageInfos[imageInfoBaseIndex].imageView = view;
                } break;
            }

            VkWriteDescriptorSet& descriptorWrite = cachedData.DescriptorWrites[bindingIndex];
            if (descriptorWrite.pBufferInfo == nullptr && descriptorWrite.pImageInfo == nullptr)
                cachedData.WritesReadyCount++;

            if (bufferInfoBaseIndex < bufferInfos.size())
                descriptorWrite.pBufferInfo = &bufferInfos[bufferInfoBaseIndex];
            if (imageInfoBaseIndex < imageInfos.size())
                descriptorWrite.pImageInfo = &imageInfos[imageInfoBaseIndex];

            // We never want to update more than one frame worth of data if we are
            // meant to be writing per frame manually
            if (static_cast<u8>(m_Info.Writability) & static_cast<u8>(DescriptorSetWritability::FrameWrite))
                break;

            frameIndex = (frameIndex + 1) % Flourish::Context::FrameBufferCount();
        }
    }

    void DescriptorSet::FlushBindings()
    {
        if (m_LastFrameWrite != 0 && !(static_cast<u8>(m_Info.Writability) & static_cast<u8>(DescriptorSetWritability::FrameWrite)))
        {
            FL_ASSERT(false, "Cannot flush descriptor set that has already been written and does not have PerFrame writability");
            return;
        }

        if (m_LastFrameWrite == Flourish::Context::FrameCount() && !(static_cast<u8>(m_Info.Writability) & static_cast<u8>(DescriptorSetWritability::MultiWrite)))
        {
            FL_ASSERT(false, "Cannot flush descriptor set twice in one frame without multiwrite enabled");
            return;
        }

        // Update the next allocation to write to if we are allowed multiple writes per frame.
        if (static_cast<u8>(m_Info.Writability) & static_cast<u8>(DescriptorSetWritability::MultiWrite))
            SwapNextAllocation();

        u32 frameIndex = Flourish::Context::FrameIndex();
        for (u32 i = 0; i < m_AllocationCount; i++)
        {
            auto& cachedData = m_AllocationCount == 1 ? m_CachedData[0] : m_CachedData[frameIndex];
            auto& writes = cachedData.DescriptorWrites;

            if (cachedData.WritesReadyCount != writes.size())
            {
                FL_ASSERT(false, "Cannot flush bindings until all binding slots have been bound");
                return;
            }
            
            vkUpdateDescriptorSets(
                Context::Devices().Device(),
                static_cast<u32>(writes.size()),
                writes.data(),
                0, nullptr
            );

            if (static_cast<u8>(m_Info.Writability) & static_cast<u8>(DescriptorSetWritability::FrameWrite))
            {
                // Reset writes for next frame
                cachedData.WritesReadyCount = 0;
                for (auto& write : writes)
                {
                    write.pBufferInfo = nullptr;
                    write.pImageInfo = nullptr;
                }
                
                // We never want to update more than one frame worth of data if we are
                // meant to be writing per frame manually
                break;
            }

            frameIndex = (frameIndex + 1) % Flourish::Context::FrameBufferCount();
        }

        // Once we've written the final set, free the cached memory
        if (!(static_cast<u8>(m_Info.Writability) & static_cast<u8>(DescriptorSetWritability::FrameWrite)))
            m_CachedData = std::vector<CachedData>();

        m_LastFrameWrite = Flourish::Context::FrameCount();
    }

    VkDescriptorSet DescriptorSet::GetSet() const
    {
        return m_AllocationCount == 1 ? m_Allocations[0].Set : m_Allocations[Flourish::Context::FrameIndex()].Set;
    }
}
