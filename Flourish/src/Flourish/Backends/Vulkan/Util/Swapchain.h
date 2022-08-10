#pragma once

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

    public:
        // TS
        static SwapChainSupportDetails GetSwapChainSupport(VkPhysicalDevice device);

    private:
        
    };
}