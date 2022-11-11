#pragma once

#include "Flourish/Api/Framebuffer.h"
#include "Flourish/Backends/Vulkan/Util/DescriptorSet.h"
#include "Flourish/Backends/Vulkan/Util/Common.h"

namespace Flourish::Vulkan
{
    class ImageViewCreateInfo;
    class Framebuffer : public Flourish::Framebuffer
    {
    public:
        Framebuffer(const FramebufferCreateInfo& createInfo);
        ~Framebuffer() override;
        
        DescriptorSet* GetPipelineDescriptorSet(std::string_view name, const DescriptorSetLayout& layout);

        // TS
        VkFramebuffer GetFramebuffer() const;
        const std::vector<VkClearValue>& GetClearValues() const { return m_CachedClearValues; }

    private:
        struct ImageData
        {
            VkImage Image;
            VkImageView ImageView;
            VmaAllocation Allocation;
            VmaAllocationInfo AllocationInfo;
        };

    private:
        void Create();
        void Cleanup();
        void PushImage(const VkImageCreateInfo& imgInfo, VkImageAspectFlagBits aspectFlags, u32 frame);

    private:
        std::array<VkFramebuffer, Flourish::Context::MaxFrameBufferCount> m_Framebuffers;
        std::array<std::vector<ImageData>, Flourish::Context::MaxFrameBufferCount> m_Images;
        std::array<std::vector<VkImageView>, Flourish::Context::MaxFrameBufferCount> m_CachedImageViews;
        std::vector<VkClearValue> m_CachedClearValues;
        std::unordered_map<std::string, std::shared_ptr<DescriptorSet>> m_PipelineDescriptorSets;
    };
}