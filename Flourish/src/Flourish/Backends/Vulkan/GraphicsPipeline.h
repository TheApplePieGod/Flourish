#pragma once

#include "Flourish/Api/GraphicsPipeline.h"
#include "Flourish/Backends/Vulkan/Util/Common.h"

namespace Flourish::Vulkan
{
    class RenderPass;
    class GraphicsPipeline : public Flourish::GraphicsPipeline
    {
    public:
        GraphicsPipeline(
            const GraphicsPipelineCreateInfo& createInfo,
            RenderPass* renderPass,
            VkSampleCountFlagBits sampleCount
        );
        ~GraphicsPipeline() override;
        
        // TS
        inline VkPipelineLayout GetLayout() const { return m_PipelineLayout; }
        inline VkPipeline GetPipeline(u32 subpassIndex) { return m_Pipelines[subpassIndex]; };

    private:
        VkPipelineLayout m_PipelineLayout = nullptr;
        std::unordered_map<u32, VkPipeline> m_Pipelines;
    };
}
