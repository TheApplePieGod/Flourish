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
        VkImageView GetAttachmentImageView(SubpassAttachment attachment) const;

        // TS
        inline VkFramebuffer GetFramebuffer() const { return m_Framebuffer; }
        inline const std::vector<VkClearValue>& GetClearValues() const { return m_CachedClearValues; }

    private:
        struct ImageData
        {
            VkImage Image = VK_NULL_HANDLE;
            VkImageView ImageView = VK_NULL_HANDLE;
            VmaAllocation Allocation = VK_NULL_HANDLE;
            VmaAllocationInfo AllocationInfo;
        };

    private:
        void Create();
        void Cleanup();
        void PushImage(const VkImageCreateInfo& imgInfo, VkImageAspectFlagBits aspectFlags);

    private:
        bool m_UseResolve;
        VkFramebuffer m_Framebuffer = VK_NULL_HANDLE;
        std::vector<ImageData> m_Images;
        std::vector<VkImageView> m_CachedImageViews;
        std::vector<VkClearValue> m_CachedClearValues;
    };
}
