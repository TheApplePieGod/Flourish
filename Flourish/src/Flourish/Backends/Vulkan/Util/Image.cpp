#include "flpch.h"
#include "Image.h"

#include "Flourish/Backends/Vulkan/Util/Context.h"

namespace Flourish::Vulkan
{
    VkImageView Image::CreateImageView(const ImageViewCreateInfo& createInfo)
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
}