#include "flpch.h"
#include "Framebuffer.h"

#include "Flourish/Backends/Vulkan/Context.h"
#include "Flourish/Backends/Vulkan/RenderPass.h"
#include "Flourish/Backends/Vulkan/Texture.h"

namespace Flourish::Vulkan
{
    Framebuffer::Framebuffer(const FramebufferCreateInfo& createInfo)
        : Flourish::Framebuffer(createInfo)
    {
        Create();
    }

    Framebuffer::~Framebuffer()
    {
        Cleanup();
    }

    VkImageView Framebuffer::GetAttachmentImageView(SubpassAttachment attachment) const
    {
        // If we have resolves then we want to return those
        u32 newIndex = attachment.AttachmentIndex;
        if (m_UseResolve)
            newIndex = newIndex * 2 + 1;
        if (attachment.Type == SubpassAttachmentType::Color)
            newIndex += m_Info.DepthAttachments.size() * (m_UseResolve ? 2 : 1);
        return m_CachedImageViews[newIndex];
    }

    void Framebuffer::Create()
    {
        RenderPass* renderPass = static_cast<RenderPass*>(m_Info.RenderPass.get());
        m_RendersToSwapchain = renderPass->RendersToSwapchain();

        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.depth = 1;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.extent.width = static_cast<u32>(m_Info.Width);
        imageInfo.extent.height = static_cast<u32>(m_Info.Height);
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        ImageViewCreateInfo viewCreateInfo;
        viewCreateInfo.MipLevels = 1;
        viewCreateInfo.LayerCount = 1;
        viewCreateInfo.AspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;

        // MSAA textures need to be resolved
        m_UseResolve = renderPass->GetSampleCount() != MsaaSampleCount::None;

        for (u32 i = 0; i < m_Info.DepthAttachments.size(); i++)
        {
            auto& attachment = m_Info.DepthAttachments[i];

            VkClearValue clearValue{};
            clearValue.depthStencil = { Flourish::Context::ReversedZBuffer() ? 0.f : 1.f, 0 };
            m_CachedClearValues.emplace_back(clearValue);
            if (m_UseResolve)
                m_CachedClearValues.emplace_back(clearValue);

            ColorFormat passFormat = renderPass->GetDepthAttachment(i).Format;
            imageInfo.format = Common::ConvertColorFormat(passFormat);
            viewCreateInfo.Format = imageInfo.format;

            // Buffer texture before resolve
            if (m_UseResolve)
            {
                imageInfo.samples = Common::ConvertMsaaSampleCount(renderPass->GetSampleCount());
                PushImage(imageInfo, VK_IMAGE_ASPECT_DEPTH_BIT);
            }

            if (attachment.Texture)
            {
                auto texture = static_cast<Texture*>(attachment.Texture.get());
                FL_ASSERT(
                    texture->GetColorFormat() == passFormat,
                    "RenderPass and Framebuffer depth attachments must have matching color formats"
                );
                FL_ASSERT(
                    texture->GetUsageType() & TextureUsageFlags::Graphics,
                    "Cannot use texture in framebuffer that does not have the graphics usage flag"
                );
                FL_ASSERT(texture->IsDepthImage(), "Framebuffer depth attachment texture must be a depth texture");
                m_CachedImageViews.push_back(
                    texture->GetLayerImageView(attachment.LayerIndex, attachment.MipLevel)
                );
            }
            else
            {
                imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
                PushImage(imageInfo, VK_IMAGE_ASPECT_DEPTH_BIT);
            } 
        }

        imageInfo.usage &= ~VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        imageInfo.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        viewCreateInfo.AspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;

        for (u32 i = 0; i < m_Info.ColorAttachments.size(); i++)
        {
            auto& attachment = m_Info.ColorAttachments[i];

            VkClearValue clearValue{};
            clearValue.color = {
                attachment.ClearColor[0],
                attachment.ClearColor[1],
                attachment.ClearColor[2],
                attachment.ClearColor[3]
            };
            m_CachedClearValues.emplace_back(clearValue);
            if (m_UseResolve)
                m_CachedClearValues.emplace_back(clearValue);

            ColorFormat passFormat = renderPass->GetColorAttachment(i).Format;
            imageInfo.format = Common::ConvertColorFormat(passFormat);
            viewCreateInfo.Format = imageInfo.format;

            // Buffer texture before resolve
            if (m_UseResolve)
            {
                imageInfo.samples = Common::ConvertMsaaSampleCount(renderPass->GetSampleCount());
                PushImage(imageInfo, VK_IMAGE_ASPECT_COLOR_BIT);
            }

            if (attachment.Texture)
            {
                auto texture = static_cast<Texture*>(attachment.Texture.get());
                FL_ASSERT(
                    texture->GetColorFormat() == passFormat,
                    "RenderPass and Framebuffer color attachments must have matching color formats"
                );
                FL_ASSERT(
                    texture->GetUsageType() & TextureUsageFlags::Graphics,
                    "Cannot use texture in framebuffer that does not have the graphics usage flag"
                );
                FL_ASSERT(
                    !(texture->GetUsageType() & TextureUsageFlags::Compute) || renderPass->GetColorAttachment(i).SupportComputeImages,
                    "Cannot use texture in framebuffer that has the compute usage flag unless the RenderPass attachment has SupportComputeImages set"
                );
                FL_ASSERT(!texture->IsDepthImage(), "Framebuffer color attachment texture must not be a depth texture");
                m_CachedImageViews.push_back(
                    texture->GetLayerImageView(attachment.LayerIndex, attachment.MipLevel)
                );
            }
            else
            {
                imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
                PushImage(imageInfo, VK_IMAGE_ASPECT_COLOR_BIT);
            } 
        }

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass->GetRenderPass();
        framebufferInfo.attachmentCount = static_cast<u32>(m_CachedImageViews.size());
        framebufferInfo.pAttachments = m_CachedImageViews.data();
        framebufferInfo.width = m_Info.Width;
        framebufferInfo.height = m_Info.Height;
        framebufferInfo.layers = 1;
        
        if (!FL_VK_CHECK_RESULT(vkCreateFramebuffer(
            Context::Devices().Device(),
            &framebufferInfo,
            nullptr,
            &m_Framebuffer
        ), "Framebuffer create framebuffer"))
            throw std::exception();
    }

    void Framebuffer::Cleanup()
    {
        auto framebuffer = m_Framebuffer;
        auto images = m_Images;
        Context::FinalizerQueue().Push([=]()
        {
            auto device = Context::Devices().Device();

            if (framebuffer)
                vkDestroyFramebuffer(device, framebuffer, nullptr);
            
            for (auto& imageData : images)
            {
                if (!imageData.Image) continue;
                vkDestroyImageView(device, imageData.ImageView, nullptr);
                vmaDestroyImage(Context::Allocator(), imageData.Image, imageData.Allocation);
            }
        }, "Framebuffer free");
    }
    
    void Framebuffer::PushImage(const VkImageCreateInfo& imgInfo, VkImageAspectFlagBits aspectFlags)
    {
        ImageData imageData;
        VmaAllocationCreateInfo allocCreateInfo{};
        allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

        if (!FL_VK_CHECK_RESULT(vmaCreateImage(
            Context::Allocator(),
            &imgInfo,
            &allocCreateInfo,
            &imageData.Image,
            &imageData.Allocation,
            &imageData.AllocationInfo
        ), "Framebuffer create image"))
            throw std::exception();

        ImageViewCreateInfo viewCreateInfo;
        viewCreateInfo.Format = imgInfo.format;
        viewCreateInfo.MipLevels = 1;
        viewCreateInfo.LayerCount = 1;
        viewCreateInfo.AspectFlags = aspectFlags;
        viewCreateInfo.Image = imageData.Image;
        imageData.ImageView = Texture::CreateImageView(viewCreateInfo);

        m_Images.emplace_back(imageData);
        m_CachedImageViews.push_back(imageData.ImageView);
    }
}
