#include "flpch.h"
#include "Texture.h"

#include "Flourish/Backends/Vulkan/Util/Context.h"
#include "Flourish/Backends/Vulkan/Buffer.h"

#ifdef FL_USE_IMGUI
#include "backends/imgui_impl_vulkan.h"
#endif

namespace Flourish::Vulkan
{
    Texture::Texture(const TextureCreateInfo& createInfo)
        : Flourish::Texture(createInfo)
    {
        m_ReadyState = new u32();

        UpdateFormat();

        // Populate initial image info
        bool hasInitialData = m_Info.InitialData && m_Info.InitialDataSize > 0;
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.depth = 1;
        imageInfo.format = m_Format;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
        if (m_Info.RenderTarget)
            imageInfo.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        if (hasInitialData)
            imageInfo.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.flags = m_Info.ArrayCount == 6 ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;

        VkImageFormatProperties formatProperties;
        vkGetPhysicalDeviceImageFormatProperties(
            Context::Devices().PhysicalDevice(),
            imageInfo.format,
            imageInfo.imageType,
            imageInfo.tiling,
            imageInfo.usage,
            imageInfo.flags,
            &formatProperties
        );

        // Clamp image sizes
        u32 newWidth = std::min(m_Info.Width, formatProperties.maxExtent.width);
        u32 newHeight = std::min(m_Info.Height, formatProperties.maxExtent.height);
        u32 newArrayCount = std::min(m_Info.ArrayCount, formatProperties.maxArrayLayers);

        // Calculate mip levels
        u32 maxMipLevels = static_cast<u32>(floor(log2(std::max(m_Info.Width, m_Info.Height)))) + 1;
        maxMipLevels = std::min(maxMipLevels, formatProperties.maxMipLevels);
        if (m_Info.MipCount == 0)
            m_MipLevels = maxMipLevels;
        else
            m_MipLevels = std::min(m_Info.MipCount, maxMipLevels);

        #if defined(FL_DEBUG) && defined(FL_LOGGING)
        if (m_MipLevels < m_Info.MipCount)
        { FL_LOG_WARN("Image was created with mip levels higher than is supported, so it was clamped to [%d]", m_MipLevels); }
        if (newWidth < m_Info.Width)
        { FL_LOG_WARN("Image was created with width higher than is supported, so it was clamped to [%d]", newWidth); }
        if (newHeight < m_Info.Height)
        { FL_LOG_WARN("Image was created with height higher than is supported, so it was clamped to [%d]", newHeight); }
        if (newArrayCount < m_Info.ArrayCount)
        { FL_LOG_WARN("Image was created with array count higher than is supported, so it was clamped to [%d]", newArrayCount); }
        #endif

        m_Info.Width = newWidth;
        m_Info.Height = newHeight;
        m_Info.ArrayCount = newArrayCount;

        VkDeviceSize imageSize = m_Info.Width * m_Info.Height * m_Info.Channels;
        u32 componentSize = BufferDataTypeSize(m_Info.DataType);
        m_ImageCount = m_Info.UsageType == BufferUsageType::Dynamic ? Flourish::Context::FrameBufferCount() : 1;
        
        CreateSampler();

        // Create staging buffer with initial data
        VkBuffer stagingBuffer;
        VmaAllocation stagingAlloc;
        VmaAllocationInfo stagingAllocInfo;
        if (hasInitialData)
        {
            #if defined(FL_DEBUG) && defined(FL_LOGGING)
            if (m_Info.UsageType == BufferUsageType::Dynamic)
            { FL_LOG_WARN("Creating dynamic texture that has initial data may not work as expected"); }
            #endif

            Buffer::AllocateStagingBuffer(
                stagingBuffer,
                stagingAlloc,
                stagingAllocInfo,
                imageSize * componentSize
            );
            memcpy(stagingAllocInfo.pMappedData, m_Info.InitialData, m_Info.InitialDataSize);
        }

        // Start a command buffer for transitioning / data transfer
        VkCommandBuffer cmdBuffer;
        Context::Commands().AllocateBuffers(GPUWorkloadType::Graphics, false, &cmdBuffer, 1);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        beginInfo.pInheritanceInfo = nullptr;

        FL_VK_ENSURE_RESULT(vkBeginCommandBuffer(cmdBuffer, &beginInfo));

        // Create images & views
        VkImageLayout currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.extent.width = static_cast<u32>(m_Info.Width);
        imageInfo.extent.height = static_cast<u32>(m_Info.Height);
        imageInfo.mipLevels = m_MipLevels;
        imageInfo.arrayLayers = m_Info.ArrayCount;
        imageInfo.initialLayout = currentLayout;
        VmaAllocationCreateInfo allocCreateInfo{};
        allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        for (u32 frame = 0; frame < m_ImageCount; frame++)
        {
            ImageData& imageData = m_Images[frame];

            // Create the image
            vmaCreateImage(
                Context::Allocator(),
                &imageInfo,
                &allocCreateInfo,
                &imageData.Image,
                &imageData.Allocation,
                &imageData.AllocationInfo
            );

            // Create the image view representing the entire texture but also
            // one for each slice of the image (mip / layer)
            ImageViewCreateInfo viewCreateInfo;
            viewCreateInfo.Image = imageData.Image;
            viewCreateInfo.Format = m_Format;
            viewCreateInfo.MipLevels = m_MipLevels;
            viewCreateInfo.LayerCount = m_Info.ArrayCount;
            imageData.ImageView = CreateImageView(viewCreateInfo);
            viewCreateInfo.LayerCount = 1;
            viewCreateInfo.MipLevels = 1;
            for (u32 i = 0; i < m_Info.ArrayCount; i++)
            {
                for (u32 j = 0; j < m_MipLevels; j++)
                {
                    viewCreateInfo.BaseArrayLayer = i;
                    viewCreateInfo.BaseMip = j;
                    VkImageView layerView = CreateImageView(viewCreateInfo);
                    imageData.SliceViews.push_back(layerView);
                    
                    #ifdef FL_USE_IMGUI
                    imageData.ImGuiHandles.push_back(ImGui_ImplVulkan_AddTexture(
                        m_Sampler,
                        layerView,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                    ));
                    #endif
                }
            }

            // If we have initial data, we need to perform the data transfer and generate
            // the mipmaps (which can only occur on the graphics queue)
            if (hasInitialData)
            {
                TransitionImageLayout(
                    imageData.Image,
                    currentLayout,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    m_MipLevels,
                    m_Info.ArrayCount,
                    cmdBuffer
                );

                Buffer::CopyBufferToImage(
                    stagingBuffer,
                    imageData.Image,
                    m_Info.Width,
                    m_Info.Height,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    cmdBuffer
                );

                auto readyState = m_ReadyState;
                GenerateMipmaps(
                    imageData.Image,
                    m_Format,
                    m_Info.Width,
                    m_Info.Height,
                    m_MipLevels,
                    m_Info.ArrayCount,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    VK_FILTER_LINEAR,
                    cmdBuffer
                );
            }
            else
            {
                TransitionImageLayout(
                    imageData.Image,
                    currentLayout,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    m_MipLevels,
                    m_Info.ArrayCount,
                    cmdBuffer
                );
            }
        }

        FL_VK_ENSURE_RESULT(vkEndCommandBuffer(cmdBuffer));

        if (m_Info.AsyncCreation)
        {
            auto readyState = m_ReadyState;
            auto callback = m_Info.CreationCallback;
            auto pushResult = Context::Queues().PushCommand(GPUWorkloadType::Graphics, cmdBuffer, [readyState, callback](){
                *readyState += 1;
                if (callback)
                    callback();
            });

            Context::DeleteQueue().PushAsync([stagingBuffer, stagingAlloc, hasInitialData, cmdBuffer]()
            {
                Context::Commands().FreeBuffer(GPUWorkloadType::Graphics, cmdBuffer);
                if (hasInitialData)
                    vmaDestroyBuffer(Context::Allocator(), stagingBuffer, stagingAlloc);
            }, pushResult.SignalSemaphore, pushResult.SignalValue, "Texture init free");
        }
        else
        {
            Context::Queues().ExecuteCommand(GPUWorkloadType::Graphics, cmdBuffer);
            *m_ReadyState += 1;
            if (m_Info.CreationCallback)
                m_Info.CreationCallback();
            Context::Commands().FreeBuffer(GPUWorkloadType::Graphics, cmdBuffer);
            if (hasInitialData)
                vmaDestroyBuffer(Context::Allocator(), stagingBuffer, stagingAlloc);
        }
    }

    Texture::Texture(const TextureCreateInfo& createInfo, VkImageView imageView)
        : Flourish::Texture(createInfo)
    {
        m_Info.MipCount = 1;
        UpdateFormat();

        m_MipLevels = 1;
        m_ImageCount = 1;
        
        ImageData& imageData = m_Images[0];
        imageData = ImageData{};
        imageData.ImageView = imageView;
        imageData.SliceViews.push_back(imageView);
    }

    Texture::~Texture()
    {
        auto imageCount = m_ImageCount;
        auto sampler = m_Sampler;
        auto images = m_Images;
        auto readyState = m_ReadyState;
        Context::DeleteQueue().Push([=]()
        {
            delete readyState;

            auto device = Context::Devices().Device();
            for (u32 frame = 0; frame < imageCount; frame++)
            {
                auto& imageData = images[frame];
                
                #ifdef FL_USE_IMGUI
                for (auto handle : imageData.ImGuiHandles)
                    ImGui_ImplVulkan_RemoveTexture((VkDescriptorSet)handle);
                #endif
                
                // Texture objects wrapping preexisting textures will not have an allocation
                // so there will be nothing to free
                if (!imageData.Allocation) continue;

                for (auto view : imageData.SliceViews)
                    vkDestroyImageView(device, view, nullptr);
                vkDestroyImageView(device, imageData.ImageView, nullptr);
                vmaDestroyImage(Context::Allocator(), imageData.Image, imageData.Allocation);
            }
            if (sampler) 
                vkDestroySampler(device, sampler, nullptr);
        }, "Texture free");
    }

    bool Texture::IsReady() const
    {
        return *m_ReadyState == 1;
    }

    #ifdef FL_USE_IMGUI
    void* Texture::GetImGuiHandle(u32 layerIndex, u32 mipLevel) const
    {
        if (m_ImageCount == 1)
            return m_Images[0].ImGuiHandles[layerIndex * m_MipLevels + mipLevel];
        return m_Images[Flourish::Context::FrameIndex()].ImGuiHandles[layerIndex * m_MipLevels + mipLevel];
    }
    #endif

    VkImage Texture::GetImage() const
    {
        if (m_ImageCount == 1) return m_Images[0].Image;
        return m_Images[Flourish::Context::FrameIndex()].Image;
    }

    VkImage Texture::GetImage(u32 frameIndex) const
    {
        if (m_ImageCount == 1) return m_Images[0].Image;
        return m_Images[frameIndex].Image;
    }

    VkImageView Texture::GetImageView() const
    {
        if (m_ImageCount == 1) return m_Images[0].ImageView;
        return m_Images[Flourish::Context::FrameIndex()].ImageView;
    }

    VkImageView Texture::GetImageView(u32 frameIndex) const
    {
        if (m_ImageCount == 1) return m_Images[0].ImageView;
        return m_Images[frameIndex].ImageView;
    }

    VkImageView Texture::GetLayerImageView(u32 layerIndex, u32 mipLevel) const
    {
        if (m_ImageCount == 1) return m_Images[0].SliceViews[layerIndex * m_MipLevels + mipLevel];
        return m_Images[Flourish::Context::FrameIndex()].SliceViews[layerIndex * m_MipLevels + mipLevel];
    }

    VkImageView Texture::GetLayerImageView(u32 frameIndex, u32 layerIndex, u32 mipLevel) const
    {
        if (m_ImageCount == 1) return m_Images[0].SliceViews[layerIndex * m_MipLevels + mipLevel];
        return m_Images[frameIndex].SliceViews[layerIndex * m_MipLevels + mipLevel];
    }

    void Texture::GenerateMipmaps(
        VkImage image,
        VkFormat imageFormat,
        u32 width,
        u32 height,
        u32 mipLevels,
        u32 layerCount,
        VkImageLayout initialLayout,
        VkImageLayout finalLayout,
        VkFilter sampleFilter,
        VkCommandBuffer buffer)
    {
        // Create and start command buffer if it wasn't passed in
        VkCommandBuffer cmdBuffer = buffer;
        if (!buffer)
        {
            Context::Commands().AllocateBuffers(GPUWorkloadType::Graphics, false, &cmdBuffer, 1);

            VkCommandBufferBeginInfo beginInfo{};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            beginInfo.pInheritanceInfo = nullptr;

            FL_VK_ENSURE_RESULT(vkBeginCommandBuffer(cmdBuffer, &beginInfo));
        }

        // Check if image format supports linear blitting
        VkFormatProperties formatProperties;
        vkGetPhysicalDeviceFormatProperties(Context::Devices().PhysicalDevice(), imageFormat, &formatProperties);

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.image = image;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = layerCount;
        barrier.subresourceRange.levelCount = 1;

        int mipWidth = static_cast<int>(width);
        int mipHeight = static_cast<int>(height);

        for (u32 i = 1; i < mipLevels; i++)
        {
            barrier.subresourceRange.baseMipLevel = i - 1;
            barrier.oldLayout = i == 1 ? initialLayout : VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

            vkCmdPipelineBarrier(cmdBuffer,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                0, nullptr,
                0, nullptr,
                1, &barrier
            );

            VkImageBlit blit{};
            blit.srcOffsets[0] = { 0, 0, 0 };
            blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
            blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.srcSubresource.mipLevel = i - 1;
            blit.srcSubresource.baseArrayLayer = 0;
            blit.srcSubresource.layerCount = layerCount;
            blit.dstOffsets[0] = { 0, 0, 0 };
            blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
            blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.dstSubresource.mipLevel = i;
            blit.dstSubresource.baseArrayLayer = 0;
            blit.dstSubresource.layerCount = layerCount;

            vkCmdBlitImage(cmdBuffer,
                image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1, &blit,
                sampleFilter
            );

            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.newLayout = finalLayout;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            vkCmdPipelineBarrier(cmdBuffer,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                0,
                0, nullptr,
                0, nullptr,
                1, &barrier
            );

            if (mipWidth > 1) mipWidth /= 2;
            if (mipHeight > 1) mipHeight /= 2;
        }

        barrier.subresourceRange.baseMipLevel = mipLevels - 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = finalLayout;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(cmdBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier
        );

        if (!buffer)
        {
            FL_VK_ENSURE_RESULT(vkEndCommandBuffer(cmdBuffer));

            auto pushResult = Context::Queues().PushCommand(GPUWorkloadType::Graphics, cmdBuffer);
            
            Context::DeleteQueue().PushAsync([cmdBuffer]()
            {
                Context::Commands().FreeBuffer(GPUWorkloadType::Graphics, cmdBuffer);
            }, pushResult.SignalSemaphore, pushResult.SignalValue, "GenerateMipmaps command free");
        }
    }

    void Texture::TransitionImageLayout(
        VkImage image,
        VkImageLayout oldLayout,
        VkImageLayout newLayout,
        u32 mipLevels,
        u32 layerCount,
        VkCommandBuffer buffer)
    {
        // Create and start command buffer if it wasn't passed in
        VkCommandBuffer cmdBuffer = buffer;
        if (!buffer)
        {
            Context::Commands().AllocateBuffers(GPUWorkloadType::Graphics, false, &cmdBuffer, 1);

            VkCommandBufferBeginInfo beginInfo{};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            beginInfo.pInheritanceInfo = nullptr;

            FL_VK_ENSURE_RESULT(vkBeginCommandBuffer(cmdBuffer, &beginInfo));
        }

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = mipLevels;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = layerCount;
        
        VkPipelineStageFlags sourceStage;
        VkPipelineStageFlags destinationStage;
        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
        {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
        {
            barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

            sourceStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_GENERAL)
        {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
        {
            barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

            sourceStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        {
            barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            sourceStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        }
        else
        {
            FL_ASSERT(false, "Invalid image layout transition");
            return;
        }

        vkCmdPipelineBarrier(
            cmdBuffer,
            sourceStage, destinationStage,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier
        );

        if (!buffer)
        {
            FL_VK_ENSURE_RESULT(vkEndCommandBuffer(cmdBuffer));

            auto pushResult = Context::Queues().PushCommand(GPUWorkloadType::Graphics, cmdBuffer);
            
            Context::DeleteQueue().PushAsync([cmdBuffer]()
            {
                Context::Commands().FreeBuffer(GPUWorkloadType::Graphics, cmdBuffer);
            }, pushResult.SignalSemaphore, pushResult.SignalValue, "TransitionImageLayout command free");
        }
    }

    VkImageView Texture::CreateImageView(const ImageViewCreateInfo& createInfo)
    {
        VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D;
        if (createInfo.LayerCount > 1)
            viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        if (createInfo.LayerCount == 6)
            viewType = VK_IMAGE_VIEW_TYPE_CUBE;

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = createInfo.Image;
        viewInfo.viewType = viewType;
        viewInfo.format = createInfo.Format;
        viewInfo.subresourceRange.aspectMask = createInfo.AspectFlags;
        viewInfo.subresourceRange.baseMipLevel = createInfo.BaseMip;
        viewInfo.subresourceRange.levelCount = createInfo.MipLevels;
        viewInfo.subresourceRange.baseArrayLayer = createInfo.BaseArrayLayer;
        viewInfo.subresourceRange.layerCount = createInfo.LayerCount;

        VkImageView view;
        FL_VK_CHECK_RESULT(vkCreateImageView(Context::Devices().Device(), &viewInfo, nullptr, &view));

        return view;
    }

    const Texture::ImageData& Texture::GetImageData() const
    {
        return m_ImageCount == 1 ? m_Images[0] : m_Images[Flourish::Context::FrameIndex()];
    }
    
    void Texture::UpdateFormat()
    {
        m_GeneralFormat = BufferDataTypeColorFormat(m_Info.DataType, m_Info.Channels);
        m_Format = Common::ConvertColorFormat(m_GeneralFormat);
        if (m_Format == VK_FORMAT_R8G8B8A8_SRGB)
            m_Format = VK_FORMAT_R8G8B8A8_UNORM; // we want to use unorm for textures
    }
    
    void Texture::CreateSampler()
    {
        FL_ASSERT(
            Flourish::Context::FeatureTable().SamplerMinMax ||
                (m_Info.SamplerState.ReductionMode != SamplerReductionMode::Min &&
                m_Info.SamplerState.ReductionMode != SamplerReductionMode::Max),
            "Texture has reduction mode of min or max, but that feature flag is not enabled"
        );

        VkSamplerReductionModeCreateInfo reductionInfo{};
        reductionInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO;
        reductionInfo.reductionMode = Common::ConvertSamplerReductionMode(m_Info.SamplerState.ReductionMode);
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.pNext = Flourish::Context::FeatureTable().SamplerMinMax ? &reductionInfo : nullptr;
        samplerInfo.magFilter = Common::ConvertSamplerFilter(m_Info.SamplerState.MagFilter);
        samplerInfo.minFilter = Common::ConvertSamplerFilter(m_Info.SamplerState.MinFilter);
        samplerInfo.addressModeU = Common::ConvertSamplerWrapMode(m_Info.SamplerState.UVWWrap[0]);
        samplerInfo.addressModeV = Common::ConvertSamplerWrapMode(m_Info.SamplerState.UVWWrap[1]);
        samplerInfo.addressModeW = Common::ConvertSamplerWrapMode(m_Info.SamplerState.UVWWrap[2]);
        samplerInfo.anisotropyEnable = Flourish::Context::FeatureTable().SamplerAnisotropy && m_Info.SamplerState.AnisotropyEnable;
        samplerInfo.maxAnisotropy = std::min(
            static_cast<float>(m_Info.SamplerState.MaxAnisotropy),
            Context::Devices().PhysicalDeviceProperties().limits.maxSamplerAnisotropy
        );
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = static_cast<float>(m_MipLevels);

        FL_VK_ENSURE_RESULT(vkCreateSampler(Context::Devices().Device(), &samplerInfo, nullptr, &m_Sampler));
    }
}