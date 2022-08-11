#pragma once

#include "Flourish/Backends/Vulkan/Util/Common.h"

namespace Flourish::Vulkan
{
    struct ImageViewCreateInfo
    {
        VkImage Image;
        VkFormat Format;
        u32 MipLevels = 1;
        u32 BaseMip = 0;
        u32 LayerCount = 1;
        u32 BaseArrayLayer = 0;
        VkImageAspectFlags AspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
    };

    struct Image
    {
        // TS
        static VkImageView CreateImageView(const ImageViewCreateInfo& createInfo);
    };
}