#pragma once

#include "Flourish/Api/RenderPass.h"
#include "Flourish/Backends/Vulkan/Util/Common.h"

namespace Flourish::Vulkan
{
    class RenderPass : public Flourish::RenderPass
    {
    public:
        RenderPass(const RenderPassCreateInfo& createInfo, bool rendersToSwapchain = false);
        ~RenderPass() override;

        // TS
        inline VkRenderPass GetRenderPass() const { return m_RenderPass; }
        inline bool RendersToSwapchain() const { return m_RendersToSwapchain; }

        inline static constexpr VkFormat DepthFormat = VK_FORMAT_D32_SFLOAT;

    protected:
        std::shared_ptr<Flourish::GraphicsPipeline> CreatePipeline(const GraphicsPipelineCreateInfo& createInfo) override;

    private:
        VkSampleCountFlagBits m_SampleCount;
        bool m_UseResolve;
        bool m_RendersToSwapchain;
        VkRenderPass m_RenderPass = nullptr;
    };
}
