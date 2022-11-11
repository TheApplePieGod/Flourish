#include "flpch.h"
#include "Framebuffer.h"

#include "Flourish/Backends/Vulkan/Util/Context.h"
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

        // Descriptor sets get implicitly cleaned up
    }

    DescriptorSet* Framebuffer::GetPipelineDescriptorSet(std::string_view name, const DescriptorSetLayout& layout)
    {
        std::string nameStr(name.data(), name.size());
        if (m_PipelineDescriptorSets.find(nameStr) == m_PipelineDescriptorSets.end())
            m_PipelineDescriptorSets[nameStr] = std::make_shared<DescriptorSet>(layout);
        return m_PipelineDescriptorSets[nameStr].get();
    }

    VkFramebuffer Framebuffer::GetFramebuffer() const
    {
        return m_Framebuffers[Flourish::Context::FrameIndex()];
    }

    void Framebuffer::Create()
    {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.depth = 1;
        imageInfo.format = RenderPass::DepthFormat;
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
        viewCreateInfo.Format = RenderPass::DepthFormat;
        viewCreateInfo.MipLevels = 1;
        viewCreateInfo.LayerCount = 1;
        viewCreateInfo.AspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;

        // MSAA textures need to be resolved
        bool useResolve = m_Info.RenderPass->GetSampleCount() != MsaaSampleCount::None;

        for (auto& attachment : m_Info.DepthAttachments)
        {
            VkClearValue clearValue{};
            clearValue.depthStencil = { Flourish::Context::ReversedZBuffer() ? 0.f : 1.f, 0 };
            m_CachedClearValues.emplace_back(clearValue);

            imageInfo.samples = Common::ConvertMsaaSampleCount(m_Info.RenderPass->GetSampleCount());

            for (u32 frame = 0; frame < Flourish::Context::FrameBufferCount(); frame++)
            {
                PushImage(imageInfo, VK_IMAGE_ASPECT_DEPTH_BIT, frame);
            }
        }

        imageInfo.usage &= ~VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        imageInfo.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        viewCreateInfo.AspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;

        u32 attachmentIndex = 0;
        for (auto& attachment : m_Info.ColorAttachments)
        {
            VkClearValue clearValue{};
            clearValue.color = {
                attachment.ClearColor[0],
                attachment.ClearColor[1],
                attachment.ClearColor[2],
                attachment.ClearColor[3]
            };
            m_CachedClearValues.emplace_back(clearValue);

            imageInfo.format = Common::ConvertColorFormat(
                m_Info.RenderPass->GetColorAttachmentColorFormat(attachmentIndex)
            );
            viewCreateInfo.Format = imageInfo.format;

            for (u32 frame = 0; frame < Flourish::Context::FrameBufferCount(); frame++)
            {
                // Create a render target sampled texture regardless of whether or not
                // a texture was passed in to the final render ouptut
                if (useResolve)
                {
                    imageInfo.samples = Common::ConvertMsaaSampleCount(m_Info.RenderPass->GetSampleCount());
                    PushImage(imageInfo, VK_IMAGE_ASPECT_COLOR_BIT, frame);
                }

                if (attachment.Texture)
                {
                    m_CachedImageViews[frame].push_back(
                        static_cast<Texture*>(attachment.Texture.get())->GetLayerImageView(
                            frame,
                            attachment.LayerIndex,
                            attachment.MipLevel
                        )
                    );
                }
                else
                {
                    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
                    PushImage(imageInfo, VK_IMAGE_ASPECT_COLOR_BIT, frame);
                } 
            }

            if (useResolve) attachmentIndex++;
            attachmentIndex++;
        }

        for (u32 frame = 0; frame < Flourish::Context::FrameBufferCount(); frame++)
        {
            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = static_cast<RenderPass*>(m_Info.RenderPass.get())->GetRenderPass();
            framebufferInfo.attachmentCount = static_cast<u32>(m_CachedImageViews[frame].size());
            framebufferInfo.pAttachments = m_CachedImageViews[frame].data();
            framebufferInfo.width = m_Info.Width;
            framebufferInfo.height = m_Info.Height;
            framebufferInfo.layers = 1;
            
            FL_VK_ENSURE_RESULT(vkCreateFramebuffer(Context::Devices().Device(), &framebufferInfo, nullptr, &m_Framebuffers[frame]));
        }
    }

    void Framebuffer::Cleanup()
    {
        auto framebuffers = m_Framebuffers;
        auto images = m_Images;
        Context::DeleteQueue().Push([=]()
        {
            auto device = Context::Devices().Device();
            for (u32 frame = 0; frame < Flourish::Context::FrameBufferCount(); frame++)
            {
                vkDestroyFramebuffer(device, framebuffers[frame], nullptr);
                
                for (auto& imageData : images[frame])
                {
                    vkDestroyImageView(device, imageData.ImageView, nullptr);
                    vmaDestroyImage(Context::Allocator(), imageData.Image, imageData.Allocation);
                }
            }
        });
    }
    
    void Framebuffer::PushImage(const VkImageCreateInfo& imgInfo, VkImageAspectFlagBits aspectFlags, u32 frame)
    {
        ImageData imageData;
        VmaAllocationCreateInfo allocCreateInfo{};
        allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

        vmaCreateImage(
            Context::Allocator(),
            &imgInfo,
            &allocCreateInfo,
            &imageData.Image,
            &imageData.Allocation,
            &imageData.AllocationInfo
        );

        ImageViewCreateInfo viewCreateInfo;
        viewCreateInfo.Format = imgInfo.format;
        viewCreateInfo.MipLevels = 1;
        viewCreateInfo.LayerCount = 1;
        viewCreateInfo.AspectFlags = aspectFlags;
        viewCreateInfo.Image = imageData.Image;
        imageData.ImageView = Texture::CreateImageView(viewCreateInfo);

        m_Images[frame].emplace_back(imageData);
        m_CachedImageViews[frame].push_back(imageData.ImageView);
    }
}