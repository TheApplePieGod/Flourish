#include "flpch.h"
#include "Swapchain.h"

#include "Flourish/Backends/Vulkan/Util/Context.h"
#include "Flourish/Backends/Vulkan/Util/Synchronization.h"

namespace Flourish::Vulkan
{
    void Swapchain::Initialize(const RenderContextCreateInfo& createInfo, VkSurfaceKHR surface)
    {
        m_Surface = surface;
        m_CurrentWidth = createInfo.Width;
        m_CurrentHeight = createInfo.Height;
        m_ClearColor = createInfo.ClearColor;

        PopulateSwapchainInfo();
        RecreateSwapchain();
        
        for (u32 frame = 0; frame < Flourish::Context::FrameBufferCount(); frame++)
            m_ImageAvailableSemaphores[frame] = Synchronization::CreateSemaphore();
    }

    void Swapchain::Shutdown()
    {
        CleanupSwapchain();
        
        auto imageAvailableSemaphores = m_ImageAvailableSemaphores;
        Context::DeleteQueue().Push([=]()
        {
            for (u32 frame = 0; frame < Flourish::Context::FrameBufferCount(); frame++)
                vkDestroySemaphore(Context::Devices().Device(), imageAvailableSemaphores[frame], nullptr);
        }, "Swapchain shutdown");
    }
    
    void Swapchain::UpdateActiveImage()
    {
        VkResult result = vkAcquireNextImageKHR(
            Context::Devices().Device(),
            m_Swapchain,
            UINT64_MAX,
            m_ImageAvailableSemaphores[Flourish::Context::FrameIndex()],
            VK_NULL_HANDLE,
            &m_ActiveImageIndex
        );
        
        // TODO: implement this
        /*if (result == VK_ERROR_OUT_OF_DATE_KHR)
            m_ShouldPresentThisFrame = false;
        else
        {
            HE_ENGINE_ASSERT(result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR);
            m_ShouldPresentThisFrame = true;
        }
        */
    }

    VkSemaphore Swapchain::GetImageAvailableSemaphore() const
    {
        return m_ImageAvailableSemaphores[Flourish::Context::FrameIndex()];
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
        u32 imageCount = Flourish::Context::FrameBufferCount();
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

        u32 queueIndices[] = { Context::Queues().QueueIndex(GPUWorkloadType::Graphics), Context::Queues().PresentQueueIndex() };
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
        std::vector<VkImage> chainImages;
        vkGetSwapchainImagesKHR(device, m_Swapchain, &imageCount, nullptr);
        chainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(device, m_Swapchain, &imageCount, chainImages.data());

        // Create renderpass compatible with all images
        RenderPassCreateInfo rpCreateInfo;
        rpCreateInfo.ColorAttachments = { { Common::RevertColorFormat(m_Info.SurfaceFormat.format) } };
        rpCreateInfo.Subpasses = {{
            {}, {{ SubpassAttachmentType::Color, 0 }}
        }};
        m_RenderPass = std::make_shared<RenderPass>(rpCreateInfo, true);

        // Populate image data
        TextureCreateInfo texCreateInfo;
        texCreateInfo.Width = m_CurrentWidth;
        texCreateInfo.Height = m_CurrentHeight;
        texCreateInfo.DataType = Texture::ColorFormatBufferDataType(rpCreateInfo.ColorAttachments[0].Format);
        FramebufferCreateInfo fbCreateInfo;
        fbCreateInfo.RenderPass = m_RenderPass;
        fbCreateInfo.Width = m_CurrentWidth;
        fbCreateInfo.Height = m_CurrentHeight;
        fbCreateInfo.ColorAttachments = {
            { m_ClearColor }
        };
        for (auto image : chainImages)
        {
            ImageData imageData{};
            imageData.Image = image;

            // Create view
            ImageViewCreateInfo viewCreateInfo;
            viewCreateInfo.Format = m_Info.SurfaceFormat.format;
            viewCreateInfo.Image = image;
            imageData.ImageView = Texture::CreateImageView(viewCreateInfo);
            
            // Create texture
            imageData.Texture = std::make_shared<Texture>(texCreateInfo, imageData.ImageView);
            
            // Create framebuffer
            fbCreateInfo.ColorAttachments[0].Texture = imageData.Texture;
            imageData.Framebuffer = std::make_shared<Framebuffer>(fbCreateInfo);
            
            m_ImageData.push_back(imageData);
        }
    }

    void Swapchain::CleanupSwapchain()
    {
        // We need to go through and reset the pointers here because we copy
        // m_ImageData in order to free the vulkan objects via the delete queue,
        // which keeps the pointers alive. That is bad because it means
        // the pointers get destructed at some arbitrary point when the lambda gets freed
        // instead of now when it is intended.
        for (auto& data : m_ImageData)
        {
            data.Framebuffer.reset();
            data.Texture.reset();
        }

        auto imageData = m_ImageData;
        auto swapchain = m_Swapchain;
        Context::DeleteQueue().Push([=]()
        {
            auto device = Context::Devices().Device();
            for (auto& data : imageData)
                vkDestroyImageView(device, data.ImageView, nullptr);
            
            vkDestroySwapchainKHR(device, swapchain, nullptr);
        }, "Swapchain free");

        m_ImageData.clear();
    }
}