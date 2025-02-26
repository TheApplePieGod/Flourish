#include "flpch.h"
#include "Swapchain.h"

#include "Flourish/Backends/Vulkan/Context.h"
#include "Flourish/Backends/Vulkan/Util/Synchronization.h"

namespace Flourish::Vulkan
{
    #ifdef FL_PLATFORM_WINDOWS
    // https://stackoverflow.com/questions/7009080/detecting-full-screen-mode-in-windows
    bool IsFullscreen(HWND windowHandle)
    {
        MONITORINFO monitorInfo{};
        monitorInfo.cbSize = sizeof(MONITORINFO);
        GetMonitorInfo(MonitorFromWindow(windowHandle, MONITOR_DEFAULTTONEAREST), &monitorInfo);

        RECT windowRect;
        GetWindowRect(windowHandle, &windowRect);

        return windowRect.left == monitorInfo.rcMonitor.left
            && windowRect.right == monitorInfo.rcMonitor.right
            && windowRect.top == monitorInfo.rcMonitor.top
            && windowRect.bottom == monitorInfo.rcMonitor.bottom;
    }
    #endif

    void Swapchain::Initialize(const RenderContextCreateInfo& createInfo, VkSurfaceKHR surface, void* windowHandle)
    {
        m_WindowHandle = windowHandle;
        m_Surface = surface;
        m_CurrentWidth = createInfo.Width;
        m_CurrentHeight = createInfo.Height;
        m_ClearColor = createInfo.ClearColor;

        PopulateSwapchainInfo();
        RecreateSwapchain();
        
        for (u32 frame = 0; frame < m_ImageAvailableSemaphores.size(); frame++)
        {
            m_ImageAvailableSemaphores[frame] = Synchronization::CreateSemaphore();
            m_ImageAvailableFences[frame] = Synchronization::CreateFence();
        }
    }

    void Swapchain::Shutdown()
    {
        CleanupSwapchain();
        
        auto imageAvailableFences = m_ImageAvailableFences;
        auto imageAvailableSemaphores = m_ImageAvailableSemaphores;
        Context::FinalizerQueue().Push([=]()
        {
            // Ensure all images are acquired before shutting down
            Synchronization::WaitForFences(imageAvailableFences.data(), m_ImageAvailableFences.size());

            for (u32 frame = 0; frame < m_ImageAvailableSemaphores.size(); frame++)
            {
                vkDestroyFence(Context::Devices().Device(), imageAvailableFences[frame], nullptr);
                vkDestroySemaphore(Context::Devices().Device(), imageAvailableSemaphores[frame], nullptr);
            }
        }, "Swapchain shutdown");
    }
    
    void Swapchain::UpdateActiveImage()
    {
        if (m_ShouldRecreate)
        {
            RecreateSwapchain();
            m_ShouldRecreate = false;
        }

        if (!m_Valid) return;

        m_SyncIndex = Flourish::Context::FrameIndex();

        VkSemaphore currentSemaphore = GetImageAvailableSemaphore();
        VkFence currentFence = GetImageAvailableFence();
        Synchronization::WaitForFences(&currentFence, 1);
        Synchronization::ResetFences(&currentFence, 1);

        VkResult result = vkAcquireNextImageKHR(
            Context::Devices().Device(),
            m_Swapchain,
            UINT64_MAX,
            currentSemaphore,
            currentFence,
            &m_ActiveImageIndex
        );
    }

    void Swapchain::UpdateDimensions(u32 width, u32 height)
    {
        m_CurrentWidth = width;
        m_CurrentHeight = height;
        m_ShouldRecreate = true;
    }

    VkSemaphore Swapchain::GetImageAvailableSemaphore() const
    {
        return m_ImageAvailableSemaphores[m_SyncIndex];
    }

    VkFence Swapchain::GetImageAvailableFence() const
    {
        return m_ImageAvailableFences[m_SyncIndex];
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
        if (!presentSupport)
        {
            FL_LOG_ERROR("Could not create RenderContext because selected device does not support presenting");
            throw std::exception();
        }
        
        // Query surface formats
        u32 formatCount = 0;
        std::vector<VkSurfaceFormatKHR> formats;
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, m_Surface, &formatCount, nullptr);
        if (formatCount != 0) {
            formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, m_Surface, &formatCount, formats.data());
        }
        if (formatCount == 0)
        {
            FL_LOG_ERROR("Could not create RenderContext because selected device does not support any surface formats");
            throw std::exception();
        }

        // Query present modes
        u32 presentModeCount = 0;
        std::vector<VkPresentModeKHR> presentModes;
        vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, m_Surface, &presentModeCount, nullptr);
        if (presentModeCount != 0) {
            presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, m_Surface, &presentModeCount, presentModes.data());
        }
        if (presentModeCount == 0)
        {
            FL_LOG_ERROR("Could not create RenderContext because selected device does not support any present modes");
            throw std::exception();
        }

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

        FL_LOG_DEBUG("Swapchain present mode is %d", m_Info.PresentMode);
    }

    void Swapchain::RecreateSwapchain()
    {
        FL_PROFILE_FUNCTION();

        if (m_Swapchain)
        {
            FL_LOG_TRACE("Swapchain %x recreating", m_Swapchain);
        }

        m_Valid = false;

        auto device = Context::Devices().Device();
        auto physicalDevice = Context::Devices().PhysicalDevice();

        VkSurfaceCapabilitiesKHR capabilities{};
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, m_Surface, &capabilities);

        if (capabilities.minImageCount == 0)
            return;

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

        if (extent.width == 0 || extent.height == 0)
            return;

        // Choose an image count (this may differ from frame buffer count but optimally it is the same)
        u32 imageCount = Flourish::Context::FrameBufferCount() + 1;
        if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount)
            imageCount = capabilities.maxImageCount;

        FL_LOG_TRACE("Creating swapchain with %d images", imageCount);

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
        if (capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)
            createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; // Transparency?
        else
            createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
        createInfo.presentMode = m_Info.PresentMode;
        createInfo.clipped = VK_TRUE;
        createInfo.oldSwapchain = m_Swapchain;

        // Fullscreen exclusive info on windows
        #ifdef FL_PLATFORM_WINDOWS
            u32 isFull = IsFullscreen((HWND)m_WindowHandle);

            VkSurfaceFullScreenExclusiveWin32InfoEXT win32ExclusiveInfo{};
            win32ExclusiveInfo.sType = VK_STRUCTURE_TYPE_SURFACE_FULL_SCREEN_EXCLUSIVE_WIN32_INFO_EXT;
            win32ExclusiveInfo.hmonitor = MonitorFromWindow((HWND)m_WindowHandle, MONITOR_DEFAULTTONEAREST);

            VkSurfaceFullScreenExclusiveInfoEXT exclusiveInfo{};
            exclusiveInfo.sType = VK_STRUCTURE_TYPE_SURFACE_FULL_SCREEN_EXCLUSIVE_INFO_EXT;
            exclusiveInfo.pNext = &win32ExclusiveInfo;
            exclusiveInfo.fullScreenExclusive = isFull ? VK_FULL_SCREEN_EXCLUSIVE_APPLICATION_CONTROLLED_EXT : VK_FULL_SCREEN_EXCLUSIVE_DEFAULT_EXT;

            if (Context::Devices().SupportsFullScreenExclusive())
                createInfo.pNext = &exclusiveInfo;
        #endif

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
        if(!FL_VK_CHECK_RESULT(vkCreateSwapchainKHR(device, &createInfo, nullptr, &newSwapchain), "RenderContext create swapchain"))
            throw std::exception();

        if (m_Swapchain)
            CleanupSwapchain();
        m_Swapchain = newSwapchain;

        // Acquire fullscreen exclusive
        // TODO: borderless mode
        #ifdef FL_PLATFORM_WINDOWS
            if (Context::Devices().SupportsFullScreenExclusive() &&
                exclusiveInfo.fullScreenExclusive == VK_FULL_SCREEN_EXCLUSIVE_APPLICATION_CONTROLLED_EXT)
                vkAcquireFullScreenExclusiveModeEXT(Context::Devices().Device(), m_Swapchain);
        #endif

        // Load images
        std::vector<VkImage> chainImages;
        vkGetSwapchainImagesKHR(device, m_Swapchain, &imageCount, nullptr);
        chainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(device, m_Swapchain, &imageCount, chainImages.data());

        // Create renderpass compatible with all images
        // Does not need to be recreated
        RenderPassCreateInfo rpCreateInfo;
        rpCreateInfo.ColorAttachments = {{
            Common::RevertColorFormat(m_Info.SurfaceFormat.format),
            AttachmentInitialization::Clear,
            true
        }};
        rpCreateInfo.Subpasses = {{
            {}, {{ SubpassAttachmentType::Color, 0 }}
        }};
        if (!m_RenderPass)
            m_RenderPass = std::make_shared<RenderPass>(rpCreateInfo);

        // Populate image data
        m_ImageData.clear();
        TextureCreateInfo texCreateInfo;
        texCreateInfo.Width = m_CurrentWidth;
        texCreateInfo.Height = m_CurrentHeight;
        texCreateInfo.Format = rpCreateInfo.ColorAttachments[0].Format;
        texCreateInfo.Usage = TextureUsageFlags::All;
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
        
        m_Valid = true;
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
        Context::FinalizerQueue().Push([=]()
        {
            auto device = Context::Devices().Device();
            for (auto& data : imageData)
                vkDestroyImageView(device, data.ImageView, nullptr);
            
            vkDestroySwapchainKHR(device, swapchain, nullptr);
        }, "Swapchain free");

        m_ImageData.clear();
    }
}
