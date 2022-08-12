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
            u32 subpassIndex
        );
        ~GraphicsPipeline() override;

        // TS
        inline VkPipeline GetPipeline() const { return m_Pipeline; }
        inline VkPipelineLayout GetLayout() const { return m_PipelineLayout; }
        inline DescriptorSet& GetDescriptorSet() { return m_DescriptorSet; }

    private:
        DescriptorSet m_DescriptorSet;
        VkPipelineLayout m_PipelineLayout;
        VkPipeline m_Pipeline;
    };
}