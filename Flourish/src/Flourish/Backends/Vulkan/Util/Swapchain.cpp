#include "flpch.h"
#include "Swapchain.h"

#include "Flourish/Backends/Vulkan/Util/Context.h"
#include "Flourish/Backends/Vulkan/Util/Image.h"

namespace Flourish::Vulkan
{
    void Swapchain::Initialize(const RenderContextCreateInfo& createInfo, VkSurfaceKHR surface)
    {
        m_Surface = surface;
        m_CurrentWidth = createInfo.Width;
        m_CurrentHeight = createInfo.Height;

        PopulateSwapchainInfo();
        RecreateSwapchain();
    }

    void Swapchain::Shutdown()
    {
        CleanupSwapchain();
    }

    void Swapchain::PopulateSwapchainInfo()
    {
        auto physicalDevice = Context::Devices().PhysicalDevice();

        VkBool32 presentSupport;
        vkGetPhysicalDeviceSurfaceSupportKHR(
            physicalDevice,
            Context::Queues().PresentQueueIndex(),
            m_Surface,
            &presentSupport
        );
        FL_CRASH_ASSERT(presentSupport, "Attempting to create a swapchain using a device that does not support presenting");
        
        // Query surface formats
        u32 formatCount = 0;
        std::vector<VkSurfaceFormatKHR> formats;
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, m_Surface, &formatCount, nullptr);
        if (formatCount != 0) {
            formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, m_Surface, &formatCount, formats.data());
        }
        FL_CRASH_ASSERT(formatCount > 0, "Attempting to create a swapchain using a device that does not support any surface formats");

        // Query present modes
        u32 presentModeCount = 0;
        std::vector<VkPresentModeKHR> presentModes;
        vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, m_Surface, &presentModeCount, nullptr);
        if (presentModeCount != 0) {
            presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, m_Surface, &presentModeCount, presentModes.data());
        }
        FL_CRASH_ASSERT(presentModeCount > 0, "Attempting to create a swapchain using a device that does not support any present modes");

        // Choose a surface format
        std::array<VkFormat, 4> preferredFormats = { VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8_UNORM, VK_FORMAT_R8G8B8_UNORM };
        VkColorSpaceKHR colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        if (formats.size() == 1) // If only VK_FORMAT_UNDEFINED is available, we can select any format
        {
            if (formats[0].format == VK_FORMAT_UNDEFINED)
                m_Info.SurfaceFormat = { preferredFormats[0], colorSpace }; // Most preferred format
            else
                m_Info.SurfaceFormat = formats[0];
        }
        else
        {
            // Search for preferred formats in order and use the first one that is available
            for (auto pf : preferredFormats)
            {
                if (m_Info.SurfaceFormat.format != VK_FORMAT_UNDEFINED) break;
                for (auto af : formats)
                {
                    if (m_Info.SurfaceFormat.format != VK_FORMAT_UNDEFINED) break;
                    if (af.format == pf && af.colorSpace == colorSpace)
                        m_Info.SurfaceFormat = af;
                }
            }

            // Otherwise use the first available
            if (m_Info.SurfaceFormat.format != VK_FORMAT_UNDEFINED)
                m_Info.SurfaceFormat = formats[0];
        }

        // Choose a present mode
        // Search for preferred modes in order and use the first one that is available
        std::array<VkPresentModeKHR, 3> preferredModes = { VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_IMMEDIATE_KHR, VK_PRESENT_MODE_FIFO_KHR };
        for (auto pm : preferredModes)
        {
            if (m_Info.PresentMode != VK_PRESENT_MODE_MAX_ENUM_KHR) break;
            for (auto am : presentModes)
            {
                if (m_Info.PresentMode != VK_PRESENT_MODE_MAX_ENUM_KHR) break;
                if (am == pm)
                    m_Info.PresentMode = pm;
            }
        }

        // Otherwise use first available
        if (m_Info.PresentMode != VK_PRESENT_MODE_MAX_ENUM_KHR)
            m_Info.PresentMode = presentModes[0];
    }

    void Swapchain::RecreateSwapchain()
    {
        auto device = Context::Devices().Device();
        auto physicalDevice = Context::Devices().PhysicalDevice();

        VkSurfaceCapabilitiesKHR capabilities{};
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, m_Surface, &capabilities);

        FL_CRASH_ASSERT(capabilities.minImageCount > 0, "Attempting to create a swapchain using a surface that doesn't have renderable images");

        // Determine the size for this swapchain
        VkExtent2D extent;
        if (capabilities.currentExtent.width != UINT32_MAX)
        {
            extent = capabilities.currentExtent;
            m_CurrentWidth = extent.width;
            m_CurrentHeight = extent.height;
        }
        else
        {
            extent = { m_CurrentWidth, m_CurrentHeight };

            extent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, extent.width));
            extent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, extent.height));
        }

        // Choose an image count (this may differ from frame buffer count but optimally it is the same)
        u32 imageCount = Flourish::Context::GetFrameBufferCount();
        if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount)
            imageCount = capabilities.maxImageCount;

        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = m_Surface;
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = m_Info.SurfaceFormat.format;
        createInfo.imageColorSpace = m_Info.SurfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        createInfo.preTransform = capabilities.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; // Transparency?
        createInfo.presentMode = m_Info.PresentMode;
        createInfo.clipped = VK_TRUE;
        createInfo.oldSwapchain = m_Swapchain;

        u32 queueIndices[] = { Context::Queues().GraphicsQueueIndex(), Context::Queues().PresentQueueIndex() };
        if (queueIndices[0] != queueIndices[1])
        {
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueIndices;
        }
        else
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VkSwapchainKHR newSwapchain;
        FL_VK_ENSURE_RESULT(vkCreateSwapchainKHR(device, &createInfo, nullptr, &newSwapchain));

        if (m_Swapchain)
            CleanupSwapchain();
        m_Swapchain = newSwapchain;

        // Load images
        vkGetSwapchainImagesKHR(device, m_Swapchain, &imageCount, nullptr);
        m_ChainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(device, m_Swapchain, &imageCount, m_ChainImages.data());

        // Create image views
        ImageViewCreateInfo viewCreateInfo;
        viewCreateInfo.Format = m_Info.SurfaceFormat.format;
        for (auto image : m_ChainImages)
        {
            viewCreateInfo.Image = image;
            m_ChainImageViews.push_back(Image::CreateImageView(viewCreateInfo));
        }
    }

    void Swapchain::CleanupSwapchain()
    {
        auto device = Context::Devices().Device();
        for (auto view : m_ChainImageViews)
            vkDestroyImageView(device, view, nullptr);
        m_ChainImageViews.clear();
        vkDestroySwapchainKHR(device, m_Swapchain, nullptr);
    }
}