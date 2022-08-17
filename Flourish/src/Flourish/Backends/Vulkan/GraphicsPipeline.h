#pragma once

#include "Flourish/Backends/Vulkan/Util/Common.h"
#include "Flourish/Backends/Vulkan/Util/DescriptorSet.h"

namespace Flourish::Vulkan
{
    class GraphicsPipeline : public Flourish::GraphicsPipeline
    {
    public:
        GraphicsPipeline(
            const GraphicsPipelineCreateInfo& createInfo,
            VkRenderPass renderPass,
            VkSampleCountFlagBits sampleCount,
            u32 subpassCount
        );
        ~GraphicsPipeline() override;

        // TS
        inline VkPipeline GetPipeline(u32 subpassIndex) const { return m_Pipelines[subpassIndex]; }
        inline VkPipelineLayout GetLayout() const { return m_PipelineLayout; }
        inline const DescriptorSetLayout& GetDescriptorSetLayout() { return m_DescriptorSetLayout; }

    private:
        DescriptorSetLayout m_DescriptorSetLayout;
        VkPipelineLayout m_PipelineLayout;
        std::vector<VkPipeline> m_Pipelines;
    };
}