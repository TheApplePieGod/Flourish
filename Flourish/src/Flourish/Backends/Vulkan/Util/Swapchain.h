#pragma once

#include "Flourish/Api/RenderContext.h"
#include "Flourish/Backends/Vulkan/Util/Common.h"
#include "Flourish/Backends/Vulkan/Texture.h"
#include "Flourish/Backends/Vulkan/Framebuffer.h"
#include "Flourish/Backends/Vulkan/RenderPass.h"
#include "Flourish/Backends/Vulkan/RenderCommandEncoder.h"

namespace Flourish::Vulkan
{
    struct SwapchainInfo
    {
        VkSurfaceFormatKHR SurfaceFormat = { VK_FORMAT_UNDEFINED };
        VkPresentModeKHR PresentMode = VK_PRESENT_MODE_MAX_ENUM_KHR;
    };

    class Swapchain
    {
    public:
        // TS
        void Initialize(const RenderContextCreateInfo& createInfo, VkSurfaceKHR surface);
        void Shutdown();

        // TS
        inline Framebuffer* GetFramebuffer() const { return m_ImageData[m_ActiveImageIndex].Framebuffer.get(); }

    private:
        struct ImageData
        {
            VkImage Image;
            VkImageView ImageView;
            std::shared_ptr<Texture> Texture;
            std::shared_ptr<Framebuffer> Framebuffer;
        };

    private:
        void PopulateSwapchainInfo();
        void RecreateSwapchain();
        void CleanupSwapchain();

    private:
        VkSwapchainKHR m_Swapchain = nullptr;
        bool m_Invalid = false;
        u32 m_CurrentWidth, m_CurrentHeight = 0;
        std::vector<ImageData> m_ImageData;
        std::shared_ptr<RenderPass> m_RenderPass;
        SwapchainInfo m_Info;
        u32 m_ActiveImageIndex = 0;

        // Copied from RenderContext so we won't need to free
        VkSurfaceKHR m_Surface;
    };
}