#pragma once

#include "Flourish/Api/Framebuffer.h"
#include "Flourish/Backends/Vulkan/Util/Common.h"

namespace Flourish::Vulkan
{
    class ImageViewCreateInfo;
    class Framebuffer : public Flourish::Framebuffer
    {
    public:
        Framebuffer(const FramebufferCreateInfo& createInfo);
        ~Framebuffer() override;

        // TS
        VkFramebuffer GetFramebuffer() const;
        VkImageView GetAttachmentImageView(SubpassAttachment attachment) const;

        // TS
        inline const std::vector<VkClearValue>& GetClearValues() const { return m_CachedClearValues; }
        inline bool RendersToSwapchain() const { return m_RendersToSwapchain; }

    private:
        struct ImageData
        {
            VkImage Image = VK_NULL_HANDLE;
            VkImageView ImageView;
            VmaAllocation Allocation;
            VmaAllocationInfo AllocationInfo;
        };

    private:
        void Create();
        void Cleanup();
        void PushImage(const VkImageCreateInfo& imgInfo, VkImageAspectFlagBits aspectFlags, u32 frame);

    private:
        bool m_UseResolve;
        bool m_RendersToSwapchain = false;
        std::array<VkFramebuffer, Flourish::Context::MaxFrameBufferCount> m_Framebuffers;
        std::array<std::vector<ImageData>, Flourish::Context::MaxFrameBufferCount> m_Images;
        std::array<std::vector<VkImageView>, Flourish::Context::MaxFrameBufferCount> m_CachedImageViews;
        std::vector<VkClearValue> m_CachedClearValues;
    };
}
