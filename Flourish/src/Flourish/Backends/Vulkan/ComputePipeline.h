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
        std::shared_ptr<Flourish::ResourceSet> CreateResourceSet(u32 setIndex, const ResourceSetCreateInfo& createInfo) override;

        // TS
        inline VkPipeline GetPipeline() const { return m_Pipeline; }
        inline VkPipelineLayout GetLayout() const { return m_PipelineLayout; }
        inline const PipelineDescriptorData* GetDescriptorData() const { return &m_DescriptorData; }

    private:
        VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;
        VkPipeline m_Pipeline = VK_NULL_HANDLE;
        PipelineDescriptorData m_DescriptorData;
    };
}
