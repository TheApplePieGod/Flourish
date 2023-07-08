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
        Texture(const TextureCreateInfo& createInfo, VkImageView imageView);
        ~Texture() override;

        // TS
        bool IsReady() const override;
        #ifdef FL_USE_IMGUI
        void* GetImGuiHandle(u32 layerIndex = 0, u32 mipLevel = 0) const override;
        #endif

        // TS
        VkImage GetImage() const;
        VkImage GetImage(u32 frameIndex) const;
        VkImageView GetImageView() const;
        VkImageView GetImageView(u32 frameIndex) const;
        VkImageView GetLayerImageView(u32 layerIndex, u32 mipLevel) const;
        VkImageView GetLayerImageView(u32 frameIndex, u32 layerIndex, u32 mipLevel) const;

        // TS
        inline VkSampler GetSampler() const { return m_Sampler; }
        inline bool IsDepthImage() const { return m_IsDepthImage; }
        
    public:
        static void GenerateMipmaps(
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
            VkCommandBuffer buffer = nullptr
        );

        // Must transtion srcImage to TRANSFER_SRC and dstImage to TRANSFER_DST
        static void Blit(
            VkImage srcImage,
            VkFormat srcFormat,
            VkImageAspectFlags srcAspect,
            u32 srcMip,
            u32 srcLayer,
            VkImage dstImage,
            VkFormat dstFormat,
            VkImageAspectFlags dstAspect,
            u32 dstMip,
            u32 dstLayer,
            u32 width,
            u32 height,
            VkFilter sampleFilter,
            VkCommandBuffer buffer = nullptr
        );

        // Can transition image to TRANSFER_DST, will happen automatically
        static void TransitionImageLayout(
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
            VkCommandBuffer buffer = nullptr
        );
        static VkImageView CreateImageView(const ImageViewCreateInfo& createInfo);

    private:
        struct ImageData
        {
            VkImage Image = nullptr;
            VkImageView ImageView = nullptr;
            VmaAllocation Allocation;
            VmaAllocationInfo AllocationInfo;
            std::vector<VkImageView> SliceViews = {};
            #ifdef FL_USE_IMGUI
            std::vector<void*> ImGuiHandles = {};
            #endif
        };

    private:
        const ImageData& GetImageData() const;
        void UpdateFormat();
        void CreateSampler();

    private:
        std::array<ImageData, Flourish::Context::MaxFrameBufferCount> m_Images;
        VkFormat m_Format;
        VkSampler m_Sampler = nullptr;
        u32 m_ImageCount = 0;
        u32* m_ReadyState = nullptr;
        bool m_IsDepthImage = false;
        bool m_IsStorageImage = false;
    
    private:
        #ifdef FL_USE_IMGUI
        inline static std::mutex s_ImGuiMutex;
        #endif
    };
}
