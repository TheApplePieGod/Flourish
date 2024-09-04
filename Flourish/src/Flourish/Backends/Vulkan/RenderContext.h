#pragma once

#include "Flourish/Api/RenderContext.h"
#include "Flourish/Backends/Vulkan/Util/Swapchain.h"
#include "Flourish/Backends/Vulkan/Util/Common.h"
#include "Flourish/Backends/Vulkan/CommandBuffer.h"

namespace Flourish::Vulkan
{
    class RenderContext : public Flourish::RenderContext
    {
    public:
        RenderContext(const RenderContextCreateInfo& createInfo);
        ~RenderContext() override;

        void UpdateDimensions(u32 width, u32 height) override;
        RenderPass* GetRenderPass() const override;
        bool Validate() override;

        [[nodiscard]] Flourish::RenderCommandEncoder* EncodeRenderCommands() override; 

        // TS
        VkFence GetSignalFence() const;
        VkSemaphore GetRenderFinishedSignalSemaphore() const;
        VkSemaphore GetSwapchainSignalSemaphore() const;

        // TS
        inline VkSurfaceKHR Surface() const { return m_Surface; }
        inline Vulkan::Swapchain& Swapchain() { return m_Swapchain; }
        inline Vulkan::CommandBuffer& CommandBuffer() { return m_CommandBuffer; }
        inline VkSemaphore GetImageAvailableSemaphore() const { return m_Swapchain.GetImageAvailableSemaphore(); }
        inline u64 GetSignalValue() const { return m_SignalValue; }

    private:
        VkSurfaceKHR m_Surface = VK_NULL_HANDLE;
        Vulkan::Swapchain m_Swapchain;
        Vulkan::CommandBuffer m_CommandBuffer;
        std::array<std::array<VkSemaphore, 2>, Flourish::Context::MaxFrameBufferCount> m_SignalSemaphores;
        std::array<VkFence, Flourish::Context::MaxFrameBufferCount> m_SignalFences;
        u64 m_SignalValue = 0;
        u64 m_LastEncodingFrame = 0;
        u64 m_LastPresentFrame = 0;
    };
}
