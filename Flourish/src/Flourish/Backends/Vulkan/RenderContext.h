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

        // TS
        [[nodiscard]] Flourish::RenderCommandEncoder* EncodeFrameRenderCommands() override; 

        // TS
        inline VkSurfaceKHR Surface() const { return m_Surface; }
        inline const Vulkan::Swapchain& Swapchain() const { return m_Swapchain; }

    private:
        VkSurfaceKHR m_Surface;
        Vulkan::Swapchain m_Swapchain;
        CommandBuffer m_CommandBuffer;
    };
}