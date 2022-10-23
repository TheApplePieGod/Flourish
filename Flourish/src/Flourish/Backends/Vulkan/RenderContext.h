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

        void Present(const std::vector<std::vector<const Flourish::CommandBuffer*>>& dependencyBuffers) override;
        RenderPass* GetRenderPass() const override;

        // TODO: this can only be allowed once per frame
        [[nodiscard]] Flourish::RenderCommandEncoder* EncodeFrameRenderCommands() override; 

        // TS
        inline VkSurfaceKHR Surface() const { return m_Surface; }
        inline const Vulkan::Swapchain& Swapchain() const { return m_Swapchain; }
        inline const Vulkan::CommandBuffer& CommandBuffer() const { return m_CommandBuffer; }
        inline VkSemaphore GetImageAvailableSemaphore() const { return m_Swapchain.GetImageAvailableSemaphore(); }

    private:
        VkSurfaceKHR m_Surface;
        Vulkan::Swapchain m_Swapchain;
        Vulkan::CommandBuffer m_CommandBuffer;
        u64 m_LastEncodingFrame = 0;
        u64 m_LastPresentFrame = 0;
    };
}