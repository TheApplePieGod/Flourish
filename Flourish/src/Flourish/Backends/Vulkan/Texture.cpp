#include "flpch.h"
#include "Texture.h"

#include "Flourish/Backends/Vulkan/Context.h"
#include "Flourish/Backends/Vulkan/Buffer.h"

#ifdef FL_USE_IMGUI
#include "backends/imgui_impl_vulkan.h"
#endif

namespace Flourish::Vulkan
{
    Texture::Texture(const TextureCreateInfo& createInfo)
        : Flourish::Texture(createInfo)
    {
        m_IsReady = std::make_shared<bool>(false);
        m_IsDepthImage = m_Info.Format == ColorFormat::Depth;
        m_Format = Common::ConvertColorFormat(m_Info.Format);
        m_IsStorageImage = m_Info.Usage & TextureUsageFlags::Compute;

        PopulateFeatures();

        // Populate initial image info
        bool hasInitialData = m_Info.InitialData && m_Info.InitialDataSize > 0;
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.depth = 1;
        imageInfo.format = m_Format;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
        if (m_Info.Usage & TextureUsageFlags::Graphics)
        {
            if (m_IsDepthImage)
                imageInfo.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
            else
                imageInfo.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
            imageInfo.usage |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
        }
        if (m_IsStorageImage)
            imageInfo.usage |= VK_IMAGE_USAGE_STORAGE_BIT;
        if (hasInitialData || m_Info.Usage & TextureUsageFlags::Transfer)
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

        if (m_MipLevels < m_Info.MipCount)
            FL_LOG_WARN("Image was created with mip levels higher than is supported, so it was clamped to [%d]", m_MipLevels);
        if (newWidth < m_Info.Width)
            FL_LOG_WARN("Image was created with width higher than is supported, so it was clamped to [%d]", newWidth);
        if (newHeight < m_Info.Height)
            FL_LOG_WARN("Image was created with height higher than is supported, so it was clamped to [%d]", newHeight);
        if (newArrayCount < m_Info.ArrayCount)
            FL_LOG_WARN("Image was created with array count higher than is supported, so it was clamped to [%d]", newArrayCount);
        
        if (newWidth == 0 || newHeight == 0 || m_MipLevels == 0)
        {
            FL_LOG_ERROR(
                "Failed to create image with invalid dimensions: W:%d, H:%d, Mips:%d",
                newWidth, newHeight, m_MipLevels
            );
            throw std::exception();
        }

        m_Info.Width = newWidth;
        m_Info.Height = newHeight;
        m_Info.ArrayCount = newArrayCount;

        VkDeviceSize imageSize = m_Info.Width * m_Info.Height * m_Channels;
        u32 componentSize = BufferDataTypeSize(ColorFormatBufferDataType(m_Info.Format));
        m_ImageCount = m_Info.Writability == TextureWritability::PerFrame ? Flourish::Context::FrameBufferCount() : 1;
        
        CreateSampler();

        // Create staging buffer with initial data
        VkBuffer stagingBuffer;
        VmaAllocation stagingAlloc;
        VmaAllocationInfo stagingAllocInfo;
        if (hasInitialData)
        {
            #if defined(FL_DEBUG) && defined(FL_LOGGING)
            if (m_Info.Writability == TextureWritability::PerFrame)
                FL_LOG_WARN("Creating per-frame writable texture that has initial data may not work as expected");
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
        auto allocInfo = Context::Commands().AllocateBuffers(GPUWorkloadType::Graphics, false, &cmdBuffer, 1, true);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        beginInfo.pInheritanceInfo = nullptr;

        if (!FL_VK_CHECK_RESULT(vkBeginCommandBuffer(cmdBuffer, &beginInfo), "Texture init command buffer begin"))
            throw std::exception();

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
            if (!FL_VK_CHECK_RESULT(vmaCreateImage(
                Context::Allocator(),
                &imageInfo,
                &allocCreateInfo,
                &imageData.Image,
                &imageData.Allocation,
                &imageData.AllocationInfo
            ), "Texture create image"))
                throw std::exception();

            // Create the image view representing the entire texture but also
            // one for each slice of the image (mip / layer)
            ImageViewCreateInfo viewCreateInfo;
            viewCreateInfo.Image = imageData.Image;
            viewCreateInfo.Format = m_Format;
            viewCreateInfo.MipLevels = m_MipLevels;
            viewCreateInfo.LayerCount = m_Info.ArrayCount;
            viewCreateInfo.AspectFlags = m_IsDepthImage ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
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
                    s_ImGuiMutex.lock();
                    imageData.ImGuiHandles.push_back((void*)ImGui_ImplVulkan_AddTexture(
                        m_Sampler,
                        layerView,
                        m_IsStorageImage ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                    ));
                    s_ImGuiMutex.unlock();
                    #endif
                }
            }

            // If we have initial data, we need to perform the data transfer and generate
            // the mipmaps (which can only occur on the graphics queue)
            VkImageAspectFlags aspect = m_IsDepthImage ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
            if (hasInitialData)
            {
                TransitionImageLayout(
                    imageData.Image,
                    currentLayout,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    aspect,
                    0, m_MipLevels,
                    0, m_Info.ArrayCount,
                    0, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                    VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                    cmdBuffer
                );

                Buffer::CopyBufferToImage(
                    stagingBuffer,
                    imageData.Image,
                    aspect,
                    m_Info.Width,
                    m_Info.Height,
                    0, 0,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    cmdBuffer
                );

                GenerateMipmaps(
                    imageData.Image,
                    m_Format,
                    aspect,
                    m_Info.Width,
                    m_Info.Height,
                    m_MipLevels,
                    m_Info.ArrayCount,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    m_IsStorageImage ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    VK_FILTER_LINEAR,
                    cmdBuffer
                );
            }
            else
            {
                TransitionImageLayout(
                    imageData.Image,
                    currentLayout,
                    m_IsStorageImage ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    aspect,
                    0, m_MipLevels,
                    0, m_Info.ArrayCount,
                    0, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                    VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                    cmdBuffer
                );
            }
        }

        if (!FL_VK_CHECK_RESULT(vkEndCommandBuffer(cmdBuffer), "Texture init command buffer end"))
            throw std::exception();

        if (m_Info.AsyncCreation)
        {
            std::weak_ptr<bool> ready = m_IsReady;
            auto callback = m_Info.CreationCallback;
            Context::Queues().PushCommand(
                GPUWorkloadType::Graphics, 
                cmdBuffer,
                [=]()
                {
                    if (auto readyPtr = ready.lock())
                        *readyPtr = true;
                    if (callback)
                        callback();
                    Context::Commands().FreeBuffer(allocInfo, cmdBuffer);
                    if (hasInitialData)
                        vmaDestroyBuffer(Context::Allocator(), stagingBuffer, stagingAlloc);
                },
                "Texture init free"
            );
        }
        else
        {
            Context::Queues().ExecuteCommand(GPUWorkloadType::Graphics, cmdBuffer);
            *m_IsReady = true;
            if (m_Info.CreationCallback)
                m_Info.CreationCallback();
            Context::Commands().FreeBuffer(allocInfo, cmdBuffer);
            if (hasInitialData)
                vmaDestroyBuffer(Context::Allocator(), stagingBuffer, stagingAlloc);
        }

        m_Initialized = true;
    }

    Texture::Texture(const TextureCreateInfo& createInfo, VkImageView imageView)
        : Flourish::Texture(createInfo)
    {
        m_Info.MipCount = 1;
        m_Format = Common::ConvertColorFormat(m_Info.Format);

        PopulateFeatures();

        m_MipLevels = 1;
        m_ImageCount = 1;
        
        ImageData& imageData = m_Images[0];
        imageData = ImageData{};
        imageData.ImageView = imageView;
        imageData.SliceViews.push_back(imageView);

        m_Initialized = true;
    }

    void Texture::operator=(Texture&& other)
    {
        Cleanup();

        Flourish::Texture::operator=(std::move(other));

        m_Images = std::move(other.m_Images);
        m_Format = other.m_Format;
        m_FeatureFlags = other.m_FeatureFlags;
        m_Sampler = other.m_Sampler;
        m_ImageCount = other.m_ImageCount;
        m_IsDepthImage = other.m_IsDepthImage;
        m_IsStorageImage = other.m_IsStorageImage;
        m_IsReady = other.m_IsReady;

        other.m_Initialized = false;
        m_Initialized = true;
    }

    Texture::~Texture()
    {
        Cleanup();
    }

    bool Texture::IsReady() const
    {
        return *m_IsReady;
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

    void Texture::Blit(
        VkImage srcImage,
        VkFormat srcFormat,
        VkFormatFeatureFlags srcFormatFeatures,
        VkImageAspectFlags srcAspect,
        u32 srcMip,
        u32 srcLayer,
        VkImage dstImage,
        VkFormat dstFormat,
        VkFormatFeatureFlags dstFormatFeatures,
        VkImageAspectFlags dstAspect,
        u32 dstMip,
        u32 dstLayer,
        u32 width,
        u32 height,
        VkFilter sampleFilter,
        VkCommandBuffer buffer)
    {
        // Create and start command buffer if it wasn't passed in
        VkCommandBuffer cmdBuffer = buffer;
        CommandBufferAllocInfo allocInfo;
        if (!buffer)
        {
            allocInfo = Context::Commands().AllocateBuffers(GPUWorkloadType::Graphics, false, &cmdBuffer, 1, true);

            VkCommandBufferBeginInfo beginInfo{};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            beginInfo.pInheritanceInfo = nullptr;

            FL_VK_ENSURE_RESULT(vkBeginCommandBuffer(cmdBuffer, &beginInfo), "Blit command buffer begin");
        }

        if ((srcFormatFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT) && (dstFormatFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT))
        {
            // If available, take advantage of the optimized blit operation

            VkImageBlit blit{};
            blit.srcOffsets[0] = { 0, 0, 0 };
            blit.srcOffsets[1] = { (int)width, (int)height, 1 };
            blit.srcSubresource.aspectMask = srcAspect;
            blit.srcSubresource.mipLevel = srcMip;
            blit.srcSubresource.baseArrayLayer = srcLayer;
            blit.srcSubresource.layerCount = 1;
            blit.dstOffsets[0] = { 0, 0, 0 };
            blit.dstOffsets[1] = { (int)width, (int)height, 1 };
            blit.dstSubresource.aspectMask = dstAspect;
            blit.dstSubresource.mipLevel = dstMip;
            blit.dstSubresource.baseArrayLayer = dstLayer;
            blit.dstSubresource.layerCount = 1;

            vkCmdBlitImage(
                cmdBuffer,
                srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1, &blit,
                sampleFilter
            );
        }
        else if ((srcFormatFeatures & VK_FORMAT_FEATURE_TRANSFER_SRC_BIT) && (dstFormatFeatures & VK_FORMAT_FEATURE_TRANSFER_DST_BIT))
        {
            // Emulate blit by first copying to a temporary buffer, then transferring the buffer into the dst texture

            u32 formatSize = ColorFormatSize(Common::RevertColorFormat(srcFormat));
            u32 bufferSize = formatSize * width * height;

            // Create a temporary buffer which will deallocate out of scope
            // TODO: this is not great, we really should have a temporary allocation for transient buffers
            BufferCreateInfo ibCreateInfo;
            ibCreateInfo.Usage = BufferUsageType::DynamicOneFrame;
            ibCreateInfo.ElementCount = 1;
            ibCreateInfo.Stride = bufferSize;
            Buffer tempBuffer(
                ibCreateInfo,
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                Buffer::MemoryDirection::CPUToGPU,
                nullptr,
                true
            );

            Buffer::CopyImageToBuffer(
                srcImage,
                srcAspect,
                tempBuffer.GetBuffer(),
                width, height,
                srcMip, srcLayer,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                cmdBuffer
            );

            VkBufferMemoryBarrier bufBarrier{};
            bufBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
            bufBarrier.buffer = tempBuffer.GetBuffer();
            bufBarrier.size = bufferSize;
            bufBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            bufBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            bufBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            bufBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            vkCmdPipelineBarrier(
                cmdBuffer,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                0,
                0, nullptr,
                1, &bufBarrier,
                0, nullptr
            );

            Buffer::CopyBufferToImage(
                tempBuffer.GetBuffer(),
                dstImage,
                dstAspect,
                width, height,
                dstMip, dstLayer,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                cmdBuffer
            );
        }
        else
        {
            // Out of luck, throw and error and do nothing
            FL_LOG_ERROR("Blit() operation performed on incompatible src and/or dst textures, skipping");
        }
        
        if (!buffer)
        {
            FL_VK_ENSURE_RESULT(vkEndCommandBuffer(cmdBuffer), "Blit command buffer end");

            Context::Queues().PushCommand(GPUWorkloadType::Graphics, cmdBuffer, [cmdBuffer, allocInfo]()
            {
                Context::Commands().FreeBuffer(allocInfo, cmdBuffer);
            }, "Blit command free");
        }
    }

    void Texture::GenerateMipmaps(
        VkImage image,
        VkFormat imageFormat,
        VkImageAspectFlags imageAspect,
        u32 width,
        u32 height,
        u32 mipLevels,
        u32 layerCount,
        VkImageLayout initialLayout,
        VkImageLayout finalLayout,
        VkFilter sampleFilter,
        VkCommandBuffer buffer)
    {
        // TODO: this function should use the Blit() function to support CmdBlit fallback,
        // but need to spend some time figuring out how to do it efficiently

        // Create and start command buffer if it wasn't passed in
        VkCommandBuffer cmdBuffer = buffer;
        CommandBufferAllocInfo allocInfo;
        if (!buffer)
        {
            allocInfo = Context::Commands().AllocateBuffers(GPUWorkloadType::Graphics, false, &cmdBuffer, 1, true);

            VkCommandBufferBeginInfo beginInfo{};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            beginInfo.pInheritanceInfo = nullptr;

            FL_VK_ENSURE_RESULT(vkBeginCommandBuffer(cmdBuffer, &beginInfo), "GenerateMipmaps command buffer begin");
        }
        
        // Expects image to be in TRANSFER_DST_OPTIMAL before proceeding
        if (initialLayout != VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
        {
            TransitionImageLayout(
                image,
                initialLayout,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                imageAspect,
                0, mipLevels,
                0, layerCount,
                // TODO: this won't work for post-compute writes, probably need
                // a layout manager in each texture
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_ACCESS_MEMORY_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                buffer
            );
        }

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.image = image;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.subresourceRange.aspectMask = imageAspect;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = layerCount;
        barrier.subresourceRange.levelCount = 1;

        int mipWidth = static_cast<int>(width);
        int mipHeight = static_cast<int>(height);

        for (u32 i = 1; i < mipLevels; i++)
        {
            barrier.subresourceRange.baseMipLevel = i - 1;
            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
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
            blit.srcSubresource.aspectMask = imageAspect;
            blit.srcSubresource.mipLevel = i - 1;
            blit.srcSubresource.baseArrayLayer = 0;
            blit.srcSubresource.layerCount = layerCount;
            blit.dstOffsets[0] = { 0, 0, 0 };
            blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
            blit.dstSubresource.aspectMask = imageAspect;
            blit.dstSubresource.mipLevel = i;
            blit.dstSubresource.baseArrayLayer = 0;
            blit.dstSubresource.layerCount = layerCount;

            vkCmdBlitImage(cmdBuffer,
                image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1, &blit,
                sampleFilter
            );

            // Transition final image to src for combined layout transition later
            if (i == mipLevels - 1)
            {
                barrier.subresourceRange.baseMipLevel = i;
                barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

                vkCmdPipelineBarrier(cmdBuffer,
                    VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                    0,
                    0, nullptr,
                    0, nullptr,
                    1, &barrier
                );
            }

            if (mipWidth > 1) mipWidth /= 2;
            if (mipHeight > 1) mipHeight /= 2;
        }

        // Edge case when there are no mips we need to transition directly from dst
        barrier.oldLayout = mipLevels == 1 ? VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL : VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout = finalLayout;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = mipLevels;
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
            FL_VK_ENSURE_RESULT(vkEndCommandBuffer(cmdBuffer), "GenerateMipmaps command buffer end");

            Context::Queues().PushCommand(GPUWorkloadType::Graphics, cmdBuffer, [cmdBuffer, allocInfo]()
            {
                Context::Commands().FreeBuffer(allocInfo, cmdBuffer);
            }, "GenerateMipmaps command free");
        }
    }

    void Texture::TransitionImageLayout(
        VkImage image,
        VkImageLayout oldLayout,
        VkImageLayout newLayout,
        VkImageAspectFlags imageAspect,
        u32 baseMip,
        u32 mipLevels,
        u32 baseLayer,
        u32 layerCount,
        VkAccessFlags srcAccessMask,
        VkPipelineStageFlags srcStage,
        VkAccessFlags dstAccessMask,
        VkPipelineStageFlags dstStage,
        VkCommandBuffer buffer)
    {
        // Create and start command buffer if it wasn't passed in
        VkCommandBuffer cmdBuffer = buffer;
        CommandBufferAllocInfo allocInfo;
        if (!buffer)
        {
            allocInfo = Context::Commands().AllocateBuffers(GPUWorkloadType::Graphics, false, &cmdBuffer, 1, true);

            VkCommandBufferBeginInfo beginInfo{};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            beginInfo.pInheritanceInfo = nullptr;

            FL_VK_ENSURE_RESULT(vkBeginCommandBuffer(cmdBuffer, &beginInfo), "TransitionImageLayout command buffer begin");
        }

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.srcAccessMask = srcAccessMask;
        barrier.dstAccessMask = dstAccessMask;
        barrier.subresourceRange.aspectMask = imageAspect;
        barrier.subresourceRange.baseMipLevel = baseMip;
        barrier.subresourceRange.levelCount = mipLevels;
        barrier.subresourceRange.baseArrayLayer = baseLayer;
        barrier.subresourceRange.layerCount = layerCount;
        
        vkCmdPipelineBarrier(
            cmdBuffer,
            srcStage, dstStage,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier
        );

        if (!buffer)
        {
            FL_VK_ENSURE_RESULT(vkEndCommandBuffer(cmdBuffer), "TransitionImageLayout command buffer end");

            Context::Queues().PushCommand(GPUWorkloadType::Graphics, cmdBuffer, [cmdBuffer, allocInfo]()
            {
                Context::Commands().FreeBuffer(allocInfo, cmdBuffer);
            }, "TransitionImageLayout command free");
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
        if (!FL_VK_CHECK_RESULT(vkCreateImageView(
            Context::Devices().Device(),
            &viewInfo,
            nullptr,
            &view
        ), "Create image view"))
            throw std::exception();

        return view;
    }

    const Texture::ImageData& Texture::GetImageData() const
    {
        return m_ImageCount == 1 ? m_Images[0] : m_Images[Flourish::Context::FrameIndex()];
    }

    void Texture::PopulateFeatures()
    {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(Context::Devices().PhysicalDevice(), m_Format, &props);

        // We only take advantage of optimal tiling so we can always assume this here
        m_FeatureFlags = props.optimalTilingFeatures;
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

        if (!FL_VK_CHECK_RESULT(vkCreateSampler(
            Context::Devices().Device(),
            &samplerInfo,
            nullptr,
            &m_Sampler
        ), "Texture create sampler"))
            throw std::exception();
    }

    void Texture::Cleanup()
    {
        if (!m_Initialized) return;
        m_Initialized = false;

        auto imageCount = m_ImageCount;
        auto sampler = m_Sampler;
        auto images = m_Images;
        Context::FinalizerQueue().Push([=]()
        {
            auto device = Context::Devices().Device();
            for (u32 frame = 0; frame < imageCount; frame++)
            {
                auto& imageData = images[frame];
                
                #ifdef FL_USE_IMGUI
                s_ImGuiMutex.lock();
                for (auto handle : imageData.ImGuiHandles)
                    if (handle)
                        ImGui_ImplVulkan_RemoveTexture((VkDescriptorSet)handle);
                s_ImGuiMutex.unlock();
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
}
