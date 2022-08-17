#pragma once

#include "Flourish/Api/Texture.h"
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

    class Texture : public Flourish::Texture
    {
    public:
        Texture(const TextureCreateInfo& createInfo);
        ~Texture() override;

        // TS
        VkImageView GetImageView() const;
        VkImageView GetImageView(u32 frameIndex) const;
        VkImageView GetLayerImageView(u32 layerIndex, u32 mipLevel) const;
        VkImageView GetLayerImageView(u32 frameIndex, u32 layerIndex, u32 mipLevel) const;

    public:
        static void GenerateMipmaps(
            VkImage image,
            VkFormat imageFormat,
            u32 width,
            u32 height,
            u32 mipLevels,
            u32 layerCount,
            VkImageLayout initialLayout,
            VkImageLayout finalLayout,
            VkFilter sampleFilter,
            VkCommandBuffer buffer = nullptr
        );
        static void TransitionImageLayout(
            VkImage image,
            VkImageLayout oldLayout,
            VkImageLayout newLayout,
            u32 mipLevels,
            u32 layerCount,
            VkCommandBuffer buffer = nullptr
        );
        static VkImageView CreateImageView(const ImageViewCreateInfo& createInfo);

    private:
        struct ImageData
        {
            VkImage Image;
            VkImageView ImageView;
            VmaAllocation Allocation;
            VmaAllocationInfo AllocationInfo;
            std::vector<VkImageView> SliceViews;
        };

    private:
        const ImageData& GetImageData() const;

    private:
        std::array<ImageData, Flourish::Context::MaxFrameBufferCount> m_Images;
        VkFormat m_Format;
        ColorFormat m_GeneralFormat;
        VkSampler m_Sampler;
        u32 m_ImageCount = 0;
        u32 m_MipLevels = 0;
    };
}