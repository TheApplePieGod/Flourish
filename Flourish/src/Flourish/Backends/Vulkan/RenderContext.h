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

        // TS
        [[nodiscard]] Flourish::RenderCommandEncoder* EncodeFrameRenderCommands() override; 

        // TS
        inline VkSurfaceKHR Surface() const { return m_Surface; }
        inline const Vulkan::Swapchain& Swapchain() const { return m_Swapchain; }
        inline const Vulkan::CommandBuffer& CommandBuffer() const { return m_CommandBuffer; }

    private:
        VkSurfaceKHR m_Surface;
        Vulkan::Swapchain m_Swapchain;
        Vulkan::CommandBuffer m_CommandBuffer;
    };
}