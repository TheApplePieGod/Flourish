#pragma once

#include "Flourish/Api/RenderPass.h"
#include "Flourish/Backends/Vulkan/Util/Common.h"

namespace Flourish::Vulkan
{
    class RenderPass : public Flourish::RenderPass
    {
    public:
        RenderPass(const RenderPassCreateInfo& createInfo);
        ~RenderPass() override;

        // TS
        inline VkRenderPass GetRenderPass() const { return m_RenderPass; }
        inline VkSampleCountFlagBits GetConvertedSampleCount() const { return m_SampleCount; }

        inline static constexpr VkFormat DepthFormat = VK_FORMAT_D32_SFLOAT;

    protected:
        std::unique_ptr<Flourish::GraphicsPipeline> CreatePipeline(const GraphicsPipelineCreateInfo& createInfo) override;

    private:
        VkSampleCountFlagBits m_SampleCount;
        bool m_UseResolve;
        VkRenderPass m_RenderPass = VK_NULL_HANDLE;
    };
}
