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
        void Initialize(const RenderContextCreateInfo& createInfo, VkSurfaceKHR surface, void* windowHandle);
        void Shutdown();
        void UpdateActiveImage();

        void UpdateDimensions(u32 width, u32 height);

        // TS
        VkSemaphore GetImageAvailableSemaphore() const;
        VkFence GetImageAvailableFence() const;
        
        // TS
        inline VkSwapchainKHR GetSwapchain() const { return m_Swapchain; }
        inline Framebuffer* GetFramebuffer() const { return m_ImageData[m_ActiveImageIndex].Framebuffer.get(); }
        inline RenderPass* GetRenderPass() const { return m_RenderPass.get(); }
        inline u32 GetActiveImageIndex() const { return m_ActiveImageIndex; }
        inline void Recreate() { m_ShouldRecreate = true; }
        inline void RecreateImmediate() { RecreateSwapchain(); m_ShouldRecreate = false; }
        inline bool IsValid() const { return m_Valid; }

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
        void* m_WindowHandle = nullptr;
        VkSwapchainKHR m_Swapchain = VK_NULL_HANDLE;
        u32 m_CurrentWidth, m_CurrentHeight = 0;
        std::array<float, 4> m_ClearColor;
        std::vector<ImageData> m_ImageData;
        std::shared_ptr<RenderPass> m_RenderPass;
        SwapchainInfo m_Info;
        u32 m_ActiveImageIndex = 0;
        u32 m_SyncIndex = 0;
        std::array<VkSemaphore, 16> m_ImageAvailableSemaphores;
        std::array<VkFence, 16> m_ImageAvailableFences;
        bool m_ShouldRecreate = false;
        bool m_Valid = true;

        // Copied from RenderContext so we won't need to free
        VkSurfaceKHR m_Surface;
    };
}
