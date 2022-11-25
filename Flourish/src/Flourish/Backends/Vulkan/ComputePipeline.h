#pragma once

#include "Flourish/Api/ComputePipeline.h"
#include "Flourish/Backends/Vulkan/Util/Common.h"
#include "Flourish/Backends/Vulkan/Util/DescriptorSet.h"

namespace Flourish::Vulkan
{
    class ComputePipeline : public Flourish::ComputePipeline
    {
    public:
        ComputePipeline(const ComputePipelineCreateInfo& createInfo);
        ~ComputePipeline() override;

        // TS
        inline VkPipeline GetPipeline() const { return m_Pipeline; }
        inline VkPipelineLayout GetLayout() const { return m_PipelineLayout; }
        inline const DescriptorSetLayout& GetDescriptorSetLayout() { return m_DescriptorSetLayout; }

    private:
        std::shared_ptr<DescriptorSet> m_DescriptorSet;
        DescriptorSetLayout m_DescriptorSetLayout;
        VkPipelineLayout m_PipelineLayout;
        VkPipeline m_Pipeline;
    };
}