#pragma once

#include "Flourish/Api/RenderContext.h"
#include "Flourish/Backends/Vulkan/Util/Common.h"

namespace Flourish::Vulkan
{
    struct SwapChainSupportDetails
    {
        VkSurfaceCapabilitiesKHR Capabilities;
        std::vector<VkSurfaceFormatKHR> Formats;
        std::vector<VkPresentModeKHR> PresentModes;
    };

    class Swapchain
    {
    public:
        // TS
        void Initialize(const RenderContextCreateInfo& createInfo);
        void Shutdown();

    public:
        // TS
        static SwapChainSupportDetails GetSwapChainSupport(VkPhysicalDevice device);

    private:
        

        // Copied from RenderContext so we don't need to delete
        VkSurfaceKHR m_Surface;
    };
}