#pragma once

#include "Flourish/Api/ComputePipeline.h"
#include "Flourish/Backends/Vulkan/Util/Common.h"

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

    private:
        VkPipelineLayout m_PipelineLayout = nullptr;
        VkPipeline m_Pipeline = nullptr;
    };
}
