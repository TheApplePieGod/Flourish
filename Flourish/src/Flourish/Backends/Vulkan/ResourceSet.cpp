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

        if (m_Info.StoreBindingReferences)
            m_StoredReferences.resize(m_ParentPool->GetBindingData().size());

        // Populate initial cache sizes
        m_CachedData.AccelWrites.resize(m_ParentPool->GetAccelStructCount());
        m_CachedData.Accels.resize(m_ParentPool->GetAccelStructCount());
        m_CachedData.BufferInfos.resize(m_ParentPool->GetBufferCount());
        m_CachedData.ImageInfos.resize(m_ParentPool->GetImageArrayElementCount());
    }

    ResourceSet::~ResourceSet()
    {
        auto pool = m_ParentPool;
        auto setList = m_SetList;
        Context::FinalizerQueue().Push([=]()
        {
            for (auto& alloc : setList)
                pool->FreeSet(alloc.Set);
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
        FL_CRASH_ASSERT(
            buffer->GetUsage() & (BufferUsageFlags::Uniform | BufferUsageFlags::Storage),
            "Buffer bind must have either 'uniform' or 'storage' usage"
        );

        ShaderResourceType bufferType = (buffer->GetUsage() & BufferUsageFlags::Uniform) ? ShaderResourceType::UniformBuffer : ShaderResourceType::StorageBuffer;
        if (!ValidateBinding(bindingIndex, bufferType, buffer, 0))
            return;

        u32 stride = buffer->GetStride();
        UpdateBinding(
            bindingIndex, 
            bufferType, 
            (const void*)buffer,
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
        
        if (!ValidateBinding(bindingIndex, texType, texture, arrayIndex))
            return;

        FL_ASSERT(
            m_ParentPool->GetBindingType(bindingIndex) != ShaderResourceType::StorageTexture || texType == ShaderResourceType::StorageTexture,
            "Attempting to bind a texture to a storage image binding that does not have the compute flag set"
        );

        UpdateBinding(
            bindingIndex, 
            texType, 
            (const void*)texture,
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

        if (!ValidateBinding(bindingIndex, texType, texture, arrayIndex))
            return;

        FL_ASSERT(
            m_ParentPool->GetBindingType(bindingIndex) != ShaderResourceType::StorageTexture || texType == ShaderResourceType::StorageTexture,
            "Attempting to bind a texture to a storage image binding that does not have the compute flag set"
        );

        UpdateBinding(
            bindingIndex, 
            texType, 
            (const void*)texture,
            true, layerIndex, mipLevel,
            arrayIndex
        );
    }

    // TODO: need to test this
    void ResourceSet::BindSubpassInput(u32 bindingIndex, const Flourish::Framebuffer* framebuffer, SubpassAttachment attachment)
    {
        FL_PROFILE_FUNCTION();

        if (!ValidateBinding(bindingIndex, ShaderResourceType::SubpassInput, &attachment, 0))
            return;
        
        VkImageView attView = static_cast<const Framebuffer*>(framebuffer)->GetAttachmentImageView(attachment);

        UpdateBinding(
            bindingIndex, 
            ShaderResourceType::SubpassInput, 
            (const void*)attView,
            attachment.Type == SubpassAttachmentType::Color,
            0, 0, 0
        );
    }

    void ResourceSet::BindAccelerationStructure(u32 bindingIndex, const Flourish::AccelerationStructure* accelStruct)
    {
        FL_PROFILE_FUNCTION();

        FL_ASSERT(accelStruct->IsBuilt(), "Must build AccelerationStructure before binding");
        
        if (!ValidateBinding(bindingIndex, ShaderResourceType::AccelerationStructure, accelStruct, 0))
            return;
        
        VkAccelerationStructureKHR accel = static_cast<const AccelerationStructure*>(accelStruct)->GetAccelStructure();

        UpdateBinding(
            bindingIndex, 
            ShaderResourceType::AccelerationStructure, 
            (const void*)accel,
            false,
            0, 0, 0
        );
    }

    void ResourceSet::SwapNextAllocation()
    {
        // Find suitable set (the set list will be fairly small in the average case)
        // TODO: this could be a little sus in some edge cases. This is totally fine for frame submissions, but
        // for persistent workloads that rebind the same set randomly, it could potentially take longer than
        // a few frames to finish. We are basically assuming that it won't happen (which it probably will not, both
        // of these scenarios need to occur.) The proper solution in the future would be to track sets in persistent command buffers
        // and free them along with it.
        AllocatedSet* set = nullptr;
        for (auto& allocated : m_SetList)
            if (Flourish::Context::FrameCount() - allocated.WriteFrame >= Flourish::Context::FrameBufferCount())
                set = &allocated;

        // Allocate new or update existing
        if (!set)
        {
            m_SetList.push_back({ m_ParentPool->AllocateSet(), Flourish::Context::FrameCount() });
            set = &m_SetList.back();
        }
        else
            set->WriteFrame = Flourish::Context::FrameCount();

        m_CurrentSet = set->Set.Set;
    }

    // Assert in debug, but silently fail in release
    // TODO: option to disable completely
    bool ResourceSet::ValidateBinding(u32 bindingIndex, ShaderResourceType resourceType, const void* resource, u32 arrayIndex)
    {
        FL_ASSERT(
            resource != nullptr,
            "Cannot bind a null resource to a resource set"
        );
        if (resource == nullptr)
            return false;

        bool bindingExists = m_ParentPool->DoesBindingExist(bindingIndex);
        FL_ASSERT(
            bindingExists,
            "Attempting to update resource binding %d that doesn't exist in the set",
            bindingIndex
        );
        if (!bindingExists)
            return false;

        bool correctType = m_ParentPool->IsResourceCorrectType(bindingIndex, resourceType);
        FL_ASSERT(
            correctType,
            "Attempting to bind a resource to %d that does not match the bind index type",
            bindingIndex
        );
        if (!correctType)
            return false;

        bool withinArray = arrayIndex < m_ParentPool->GetBindingData()[bindingIndex].ArrayCount;
        FL_ASSERT(
            withinArray,
            "Attempting to bind a resource to %d with an out of range arrayIndex %d",
            bindingIndex,
            arrayIndex
        );
        if (!withinArray)
            return false;

        return true;
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

        // Update descriptor information
        auto& bindingData = m_ParentPool->GetBindingData()[bindingIndex];
        u32 bufferInfoBaseIndex = bindingData.BufferArrayIndex;
        u32 imageInfoBaseIndex = bindingData.ImageArrayIndex;
        auto& cachedData = m_CachedData;
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

                bufferInfos[bufferInfoBaseIndex + arrayIndex].buffer = buffer->GetGPUBuffer();
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
                    imageInfos[imageInfoBaseIndex + arrayIndex].imageView = texture->GetLayerImageView(offset, size); 
                else
                    imageInfos[imageInfoBaseIndex + arrayIndex].imageView = texture->GetImageView();
            } break;

            case ShaderResourceType::SubpassInput:
            {
                VkImageView view = (VkImageView)resource;

                // useOffset: is the attachment a color attachment
                imageInfos[imageInfoBaseIndex].sampler = VK_NULL_HANDLE;
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
    }

    void ResourceSet::FlushBindings()
    {
        FL_PROFILE_FUNCTION();

        // Update the next allocation to write to
        SwapNextAllocation();

        auto& cachedData = m_CachedData;
        auto& writes = cachedData.DescriptorWrites;

        VkDescriptorSet set = m_CurrentSet;
        for (auto& write : writes)
            write.dstSet = set;

        vkUpdateDescriptorSets(
            Context::Devices().Device(),
            static_cast<u32>(writes.size()),
            writes.data(),
            0, nullptr
        );

        // TODO: find a way to reuse these?
        m_CachedData.DescriptorWrites.clear();
    }
}
