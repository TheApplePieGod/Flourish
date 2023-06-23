#include "flpch.h"
#include "ResourceSet.h"

#include "Flourish/Backends/Vulkan/Buffer.h"
#include "Flourish/Backends/Vulkan/Texture.h"
#include "Flourish/Backends/Vulkan/Framebuffer.h"
#include "Flourish/Backends/Vulkan/RayTracing/AccelerationStructure.h"
#include "Flourish/Backends/Vulkan/Context.h"

namespace Flourish::Vulkan
{
    void ResourceSet::StoredReferences::Clear()
    {
        Buffer = nullptr;
        Texture = nullptr;
        Framebuffer = nullptr;
        AccelStruct = nullptr;
    }

    ResourceSet::ResourceSet(
        const ResourceSetCreateInfo& createInfo,
        ResourceSetPipelineCompatability compatability,
        const std::shared_ptr<DescriptorPool>& parentPool
    )
        : Flourish::ResourceSet(createInfo, compatability), m_ParentPool(parentPool)
    {
        FL_PROFILE_FUNCTION();

        FL_ASSERT(
            static_cast<u8>(m_Info.Writability) & static_cast<u8>(ResourceSetWritability::_FrameWrite) ||
            !(static_cast<u8>(m_Info.Writability) & static_cast<u8>(ResourceSetWritability::_MultiWrite)),
            "Multiwrite cannot be enabled on a resource set if framewrite is not also enabled"
        );

        if (m_Info.StoreBindingReferences)
            m_StoredReferences.resize(m_ParentPool->GetBindingData().size());

        if (static_cast<u8>(m_Info.Writability) & static_cast<u8>(ResourceSetWritability::_DynamicData))
            m_AllocationCount = Flourish::Context::FrameBufferCount();
        for (u32 i = 0; i < m_AllocationCount; i++)
        {
            m_Allocations[i] = m_ParentPool->AllocateSet();

            // Ensure free lists are populated with initial allocations when using multiwrite
            if (static_cast<u8>(m_Info.Writability) & static_cast<u8>(ResourceSetWritability::_MultiWrite))
            {
                m_SetLists[i].Sets.push_back(m_Allocations[i]);
                m_SetLists[i].FreeIndex++;
            }
        }

        // Only need extra cache when we write all frames at once
        u32 cacheCount = m_Info.Writability == ResourceSetWritability::OnceDynamicData ? m_AllocationCount : 1;
        for (u32 i = 0; i < cacheCount; i++)
        {
            m_CachedData.emplace_back();
            auto& data = m_CachedData.back();

            if (compatability & ResourceSetPipelineCompatabilityFlags::RayTracing)
            {
                data.AccelWrites.resize(m_ParentPool->GetAccelStructCount());
                data.Accels.resize(m_ParentPool->GetAccelStructCount());
            }

            data.BufferInfos.resize(m_ParentPool->GetBufferCount());
            data.ImageInfos.resize(m_ParentPool->GetImageArrayElementCount());
        }
    }

    ResourceSet::~ResourceSet()
    {
        auto allocs = m_Allocations;
        auto allocCount = m_AllocationCount;
        auto pool = m_ParentPool;
        auto writability = m_Info.Writability;
        auto setLists = m_SetLists;
        Context::FinalizerQueue().Push([=]()
        {
            if (static_cast<u8>(writability) & static_cast<u8>(ResourceSetWritability::_MultiWrite))
            {
                for (u32 i = 0; i < allocCount; i++)
                    for (auto& alloc : setLists[i].Sets)
                        pool->FreeSet(alloc);
            }
            else
                for (u32 i = 0; i < allocCount; i++)
                    pool->FreeSet(allocs[i]);
            
        }, "ResourceSet free");
    }

    void ResourceSet::BindBuffer(u32 bindingIndex, const std::shared_ptr<Flourish::Buffer>& buffer, u32 bufferOffset, u32 elementCount)
    {
        FL_CRASH_ASSERT(!m_Info.StoreBindingReferences || bindingIndex < m_StoredReferences.size(), "Binding index out of range");

        if (m_Info.StoreBindingReferences)
        {
            m_StoredReferences[bindingIndex].Clear();
            m_StoredReferences[bindingIndex].Buffer = buffer;
        }

        BindBuffer(bindingIndex, buffer.get(), bufferOffset, elementCount);
    }

    void ResourceSet::BindTexture(u32 bindingIndex, const std::shared_ptr<Flourish::Texture>& texture, u32 arrayIndex)
    {
        if (m_Info.StoreBindingReferences)
        {
            FL_CRASH_ASSERT(bindingIndex < m_StoredReferences.size(), "Binding index out of range");

            m_StoredReferences[bindingIndex].Clear();
            m_StoredReferences[bindingIndex].Texture = texture;
        }

        BindTexture(bindingIndex, texture.get(), arrayIndex);
    }
    
    void ResourceSet::BindTextureLayer(u32 bindingIndex, const std::shared_ptr<Flourish::Texture>& texture, u32 layerIndex, u32 mipLevel, u32 arrayIndex)
    {
        if (m_Info.StoreBindingReferences)
        {
            FL_CRASH_ASSERT(bindingIndex < m_StoredReferences.size(), "Binding index out of range");

            m_StoredReferences[bindingIndex].Clear();
            m_StoredReferences[bindingIndex].Texture = texture;
        }

        BindTextureLayer(bindingIndex, texture.get(), layerIndex, mipLevel, arrayIndex);
    }

    void ResourceSet::BindSubpassInput(u32 bindingIndex, const std::shared_ptr<Flourish::Framebuffer>& framebuffer, SubpassAttachment attachment)
    {
        if (m_Info.StoreBindingReferences)
        {
            FL_CRASH_ASSERT(bindingIndex < m_StoredReferences.size(), "Binding index out of range");

            m_StoredReferences[bindingIndex].Clear();
            m_StoredReferences[bindingIndex].Framebuffer = framebuffer;
        }

        BindSubpassInput(bindingIndex, framebuffer.get(), attachment);
    }

    void ResourceSet::BindAccelerationStructure(u32 bindingIndex, const std::shared_ptr<Flourish::AccelerationStructure>& accelStruct)
    {
        if (m_Info.StoreBindingReferences)
        {
            FL_CRASH_ASSERT(bindingIndex < m_StoredReferences.size(), "Binding index out of range");

            m_StoredReferences[bindingIndex].Clear();
            m_StoredReferences[bindingIndex].AccelStruct = accelStruct;
        }

        BindAccelerationStructure(bindingIndex, accelStruct.get());
    }

    void ResourceSet::BindBuffer(u32 bindingIndex, const Flourish::Buffer* buffer, u32 bufferOffset, u32 elementCount)
    {
        FL_PROFILE_FUNCTION();

        FL_CRASH_ASSERT(elementCount + bufferOffset <= buffer->GetAllocatedCount(), "ElementCount + BufferOffset must be <= buffer allocated count");
        FL_CRASH_ASSERT(buffer->GetType() == BufferType::Uniform || buffer->GetType() == BufferType::Storage, "Buffer bind must be either a uniform or storage buffer");

        ShaderResourceType bufferType = buffer->GetType() == BufferType::Uniform ? ShaderResourceType::UniformBuffer : ShaderResourceType::StorageBuffer;
        ValidateBinding(bindingIndex, bufferType, buffer, 0);

        if (!(static_cast<u8>(m_Info.Writability) & static_cast<u8>(ResourceSetWritability::_DynamicData)) && buffer->GetUsage() == BufferUsageType::Dynamic)
        {
            FL_LOG_ERROR("Cannot bind a dynamic buffer to a resource set with static writability");
            throw std::exception();
        }

        u32 stride = buffer->GetStride();
        UpdateBinding(
            bindingIndex, 
            bufferType, 
            buffer,
            true,
            stride * bufferOffset,
            stride * elementCount,
            0
        );
    }

    // TODO: ensure bound texture is not also being written to in framebuffer
    void ResourceSet::BindTexture(u32 bindingIndex, const Flourish::Texture* texture, u32 arrayIndex)
    {
        FL_PROFILE_FUNCTION();

        ShaderResourceType texType =
            (texture->GetUsageType() & TextureUsageFlags::Compute)
                ? ShaderResourceType::StorageTexture
                : ShaderResourceType::Texture;
        
        ValidateBinding(bindingIndex, texType, texture, arrayIndex);
        FL_ASSERT(
            m_ParentPool->GetBindingType(bindingIndex) != ShaderResourceType::StorageTexture || texType == ShaderResourceType::StorageTexture,
            "Attempting to bind a texture to a storage image binding that does not have the compute flag set"
        );

        if (!(static_cast<u8>(m_Info.Writability) & static_cast<u8>(ResourceSetWritability::_DynamicData)) && texture->GetWritability() == TextureWritability::PerFrame)
        {
            FL_LOG_ERROR("Cannot bind a dynamic texture to a resource set with static writability");
            throw std::exception();
        }

        UpdateBinding(
            bindingIndex, 
            texType, 
            texture,
            false, 0, 0,
            arrayIndex
        );
    }
    
    // TODO: ensure bound texture is not also being written to in framebuffer
    void ResourceSet::BindTextureLayer(u32 bindingIndex, const Flourish::Texture* texture, u32 layerIndex, u32 mipLevel, u32 arrayIndex)
    {
        FL_PROFILE_FUNCTION();

        ShaderResourceType texType =
            (texture->GetUsageType() & TextureUsageFlags::Compute)
                ? ShaderResourceType::StorageTexture
                : ShaderResourceType::Texture;

        ValidateBinding(bindingIndex, texType, texture, arrayIndex);
        FL_ASSERT(
            m_ParentPool->GetBindingType(bindingIndex) != ShaderResourceType::StorageTexture || texType == ShaderResourceType::StorageTexture,
            "Attempting to bind a texture to a storage image binding that does not have the compute flag set"
        );

        UpdateBinding(
            bindingIndex, 
            texType, 
            texture,
            true, layerIndex, mipLevel,
            arrayIndex
        );
    }

    // TODO: need to test this
    void ResourceSet::BindSubpassInput(u32 bindingIndex, const Flourish::Framebuffer* framebuffer, SubpassAttachment attachment)
    {
        FL_PROFILE_FUNCTION();
        
        // This could be a tighter bound (i.e. also work with OnceDynamicData) if
        // we modified UpdateBinding slightly somehow
        if (!(static_cast<u8>(m_Info.Writability) & static_cast<u8>(ResourceSetWritability::_FrameWrite)))
        {
            FL_LOG_ERROR("Cannot bind a subpass input to a resource set without PerFrame writability");
            throw std::exception();
        }

        ValidateBinding(bindingIndex, ShaderResourceType::SubpassInput, &attachment, 0);
        
        VkImageView attView = static_cast<const Framebuffer*>(framebuffer)->GetAttachmentImageView(attachment);

        UpdateBinding(
            bindingIndex, 
            ShaderResourceType::SubpassInput, 
            attView,
            attachment.Type == SubpassAttachmentType::Color,
            0, 0, 0
        );
    }

    void ResourceSet::BindAccelerationStructure(u32 bindingIndex, const Flourish::AccelerationStructure* accelStruct)
    {
        FL_PROFILE_FUNCTION();

        FL_ASSERT(accelStruct->IsBuilt(), "Must build AccelerationStructure before binding");
        
        ValidateBinding(bindingIndex, ShaderResourceType::AccelerationStructure, accelStruct, 0);
        
        VkAccelerationStructureKHR accel = static_cast<const AccelerationStructure*>(accelStruct)->GetAccelStructure();

        UpdateBinding(
            bindingIndex, 
            ShaderResourceType::AccelerationStructure, 
            accel,
            false,
            0, 0, 0
        );
    }

    void ResourceSet::SwapNextAllocation()
    {
        auto& list = m_SetLists[Flourish::Context::FrameIndex()];

        // New frame, reset free list pointer
        if (m_LastFrameWrite != Flourish::Context::FrameCount())
            list.FreeIndex = 0;

        // Update allocation
        if (list.FreeIndex >= list.Sets.size())
            list.Sets.push_back(m_ParentPool->AllocateSet());
        m_Allocations[Flourish::Context::FrameIndex()] = list.Sets[list.FreeIndex++];
    }

    // TODO: might want to disable this in release?
    void ResourceSet::ValidateBinding(u32 bindingIndex, ShaderResourceType resourceType, const void* resource, u32 arrayIndex)
    {
        if (resource == nullptr)
        {
            FL_LOG_ERROR("Cannot bind a null resource to a resource set");
            throw std::exception();
        }

        if (!m_ParentPool->DoesBindingExist(bindingIndex))
        {
            FL_LOG_ERROR("Attempting to update resource binding %d that doesn't exist in the set", bindingIndex);
            throw std::exception();
        }

        if (!m_ParentPool->IsResourceCorrectType(bindingIndex, resourceType))
        {
            FL_LOG_ERROR("Attempting to bind a resource that does not match the bind index type");
            throw std::exception();
        }

        if (m_ParentPool->GetBindingData()[bindingIndex].ArrayCount <= arrayIndex)
        {
            FL_LOG_ERROR("Attempting to bind a resource with an out of range arrayIndex");
            throw std::exception();
        }
    }

    void ResourceSet::UpdateBinding(
        u32 bindingIndex,
        ShaderResourceType resourceType,
        const void* resource,
        bool useOffset,
        u32 offset,
        u32 size,
        u32 arrayIndex
    )
    {
        FL_PROFILE_FUNCTION();

        if (m_LastFrameWrite != 0 && !(static_cast<u8>(m_Info.Writability) & static_cast<u8>(ResourceSetWritability::_FrameWrite)))
        {
            FL_ASSERT(false, "Cannot update resource set that has already been written and does not have PerFrame writability");
            return;
        }
        
        // Update descriptor information. The for loop is structured such that the
        // following behavior occurs for different writabilities:
        //     - OnceStaticData: one cache entry is written, frame index won't matter
        //                       since we have already validated that resource is static
        //     - OnceDynamicData: cache entry is written for each frameindex
        //     - PerFrame: cache entry for current frameindex is written
        auto& bindingData = m_ParentPool->GetBindingData()[bindingIndex];
        u32 bufferInfoBaseIndex = bindingData.BufferArrayIndex;
        u32 imageInfoBaseIndex = bindingData.ImageArrayIndex;
        u32 frameIndex = Flourish::Context::FrameIndex();
        for (u32 i = 0; i < m_AllocationCount; i++)
        {
            auto& cachedData = m_CachedData.size() == 1 ? m_CachedData[0] : m_CachedData[frameIndex];
            auto& bufferInfos = cachedData.BufferInfos;
            auto& imageInfos = cachedData.ImageInfos;

            VkWriteDescriptorSet descriptorWrite{};
            descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrite.dstBinding = bindingIndex;
            descriptorWrite.dstArrayElement = arrayIndex;
            descriptorWrite.descriptorType = bindingData.Type;
            descriptorWrite.descriptorCount = 1;

            if (bufferInfoBaseIndex < bufferInfos.size())
                descriptorWrite.pBufferInfo = &bufferInfos[bufferInfoBaseIndex + arrayIndex];
            if (imageInfoBaseIndex < imageInfos.size())
                descriptorWrite.pImageInfo = &imageInfos[imageInfoBaseIndex + arrayIndex];

            switch (resourceType)
            {
                default: { FL_ASSERT(false, "Cannot update resource set with selected resource type"); } break;

                case ShaderResourceType::UniformBuffer:
                case ShaderResourceType::StorageBuffer:
                {
                    const Buffer* buffer = static_cast<const Buffer*>(resource);

                    bufferInfos[bufferInfoBaseIndex + arrayIndex].buffer = buffer->GetBuffer(frameIndex);
                    bufferInfos[bufferInfoBaseIndex + arrayIndex].offset = offset;
                    bufferInfos[bufferInfoBaseIndex + arrayIndex].range = size;
                } break;

                case ShaderResourceType::Texture:
                case ShaderResourceType::StorageTexture:
                {
                    const Texture* texture = static_cast<const Texture*>(resource);

                    imageInfos[imageInfoBaseIndex + arrayIndex].sampler = texture->GetSampler();
                    imageInfos[imageInfoBaseIndex + arrayIndex].imageLayout = resourceType == ShaderResourceType::Texture ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_GENERAL;
                    if (useOffset)
                        // size is the mip level here
                        imageInfos[imageInfoBaseIndex + arrayIndex].imageView = texture->GetLayerImageView(frameIndex, offset, size); 
                    else
                        imageInfos[imageInfoBaseIndex + arrayIndex].imageView = texture->GetImageView(frameIndex);
                } break;

                case ShaderResourceType::SubpassInput:
                {
                    VkImageView view = (VkImageView)resource;

                    // useOffset: is the attachment a color attachment
                    imageInfos[imageInfoBaseIndex].sampler = NULL;
                    imageInfos[imageInfoBaseIndex].imageLayout = useOffset ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL;
                    imageInfos[imageInfoBaseIndex].imageView = view;
                } break;

                case ShaderResourceType::AccelerationStructure:
                {
                    VkAccelerationStructureKHR accel = (VkAccelerationStructureKHR)resource;
                    u32 accelBaseIndex = m_ParentPool->GetBindingData()[bindingIndex].AccelArrayIndex;
                    cachedData.Accels[accelBaseIndex + arrayIndex] = accel;

                    cachedData.AccelWrites[accelBaseIndex + arrayIndex] = {
                        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
                        nullptr,
                        1,
                        &cachedData.Accels[accelBaseIndex + arrayIndex]
                    };
                    descriptorWrite.pNext = &cachedData.AccelWrites[accelBaseIndex + arrayIndex];
                } break;
            }

            cachedData.DescriptorWrites.emplace_back(descriptorWrite);

            // We never want to update more than one frame worth of data if we are
            // meant to be writing per frame manually
            if (static_cast<u8>(m_Info.Writability) & static_cast<u8>(ResourceSetWritability::_FrameWrite))
                break;

            frameIndex = (frameIndex + 1) % Flourish::Context::FrameBufferCount();
        }
    }

    void ResourceSet::FlushBindings()
    {
        FL_PROFILE_FUNCTION();

        if (m_LastFrameWrite != 0 && !(static_cast<u8>(m_Info.Writability) & static_cast<u8>(ResourceSetWritability::_FrameWrite)))
        {
            FL_ASSERT(false, "Cannot flush resource set that has already been written and does not have PerFrame writability");
            return;
        }

        if (m_LastFrameWrite == Flourish::Context::FrameCount() && !(static_cast<u8>(m_Info.Writability) & static_cast<u8>(ResourceSetWritability::_MultiWrite)))
        {
            FL_ASSERT(false, "Cannot flush resource set twice in one frame without multiwrite enabled");
            return;
        }

        // Update the next allocation to write to if we are allowed multiple writes per frame.
        if (static_cast<u8>(m_Info.Writability) & static_cast<u8>(ResourceSetWritability::_MultiWrite))
            SwapNextAllocation();

        u32 frameIndex = Flourish::Context::FrameIndex();
        for (u32 i = 0; i < m_AllocationCount; i++)
        {
            auto& cachedData = m_CachedData.size() == 1 ? m_CachedData[0] : m_CachedData[frameIndex];
            auto& writes = cachedData.DescriptorWrites;

            // Will always be zero since swapping only happens in multiwrites
            VkDescriptorSet set = GetSet(frameIndex);
            for (auto& write : writes)
                write.dstSet = set;

            vkUpdateDescriptorSets(
                Context::Devices().Device(),
                static_cast<u32>(writes.size()),
                writes.data(),
                0, nullptr
            );

            if (static_cast<u8>(m_Info.Writability) & static_cast<u8>(ResourceSetWritability::_FrameWrite))
            {
                cachedData.DescriptorWrites.clear();
                
                // We never want to update more than one frame worth of data if we are
                // meant to be writing per frame manually
                break;
            }

            frameIndex = (frameIndex + 1) % Flourish::Context::FrameBufferCount();
        }

        // Once we've written the final set, free the cached memory
        if (!(static_cast<u8>(m_Info.Writability) & static_cast<u8>(ResourceSetWritability::_FrameWrite)))
            m_CachedData = std::vector<CachedData>();

        m_LastFrameWrite = Flourish::Context::FrameCount();
    }

    VkDescriptorSet ResourceSet::GetSet() const
    {
        return GetSet(Flourish::Context::FrameIndex());
    }

    VkDescriptorSet ResourceSet::GetSet(u32 frameIndex) const
    {
        return m_AllocationCount == 1 ? m_Allocations[0].Set : m_Allocations[frameIndex].Set;
    }
}
