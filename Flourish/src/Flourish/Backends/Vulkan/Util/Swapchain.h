#pragma once

#include "Flourish/Api/RenderContext.h"
#include "Flourish/Backends/Vulkan/Util/Common.h"

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

    private:
        void PopulateSwapchainInfo();
        void RecreateSwapchain();
        void CleanupSwapchain();

    private:
        VkSwapchainKHR m_Swapchain = nullptr;
        bool m_Invalid = false;
        u32 m_CurrentWidth, m_CurrentHeight = 0;
        std::vector<VkImage> m_ChainImages;
        std::vector<VkImageView> m_ChainImageViews;
        SwapchainInfo m_Info;

        // Copied from RenderContext so we don't need to delete
        VkSurfaceKHR m_Surface;
    };
}