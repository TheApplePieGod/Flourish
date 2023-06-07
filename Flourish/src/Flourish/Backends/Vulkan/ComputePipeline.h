#pragma once

#include "Flourish/Api/ComputePipeline.h"
#include "Flourish/Backends/Vulkan/Util/Common.h"
#include "Flourish/Backends/Vulkan/Util/DescriptorBinder.h"

namespace Flourish::Vulkan
{
    class ComputePipeline : public Flourish::ComputePipeline
    {
    public:
        ComputePipeline(const ComputePipelineCreateInfo& createInfo);
        ~ComputePipeline() override;

        // TS
        std::shared_ptr<Flourish::DescriptorSet> CreateDescriptorSet(u32 setIndex, const DescriptorSetCreateInfo& createInfo) override;

        // TS
        inline VkPipeline GetPipeline() const { return m_Pipeline; }
        inline VkPipelineLayout GetLayout() const { return m_PipelineLayout; }
        inline const PipelineDescriptorData* GetDescriptorData() const { return &m_DescriptorData; }

    private:
        VkPipelineLayout m_PipelineLayout = nullptr;
        VkPipeline m_Pipeline = nullptr;
        PipelineDescriptorData m_DescriptorData;
    };
}
