#pragma once

#include "Flourish/Api/GraphicsPipeline.h"
#include "Flourish/Backends/Vulkan/Util/Common.h"
#include "Flourish/Backends/Vulkan/Util/DescriptorBinder.h"

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
        std::shared_ptr<Flourish::DescriptorSet> CreateDescriptorSet(const DescriptorSetCreateInfo& createInfo) override;
        
        // TS
        inline VkPipelineLayout GetLayout() const { return m_PipelineLayout; }
        inline VkPipeline GetPipeline(u32 subpassIndex) { return m_Pipelines[subpassIndex]; };
        inline const PipelineDescriptorData* GetDescriptorData() const { return &m_DescriptorData; }

    private:
        VkPipelineLayout m_PipelineLayout = nullptr;
        std::unordered_map<u32, VkPipeline> m_Pipelines;
        PipelineDescriptorData m_DescriptorData;
    };
}
