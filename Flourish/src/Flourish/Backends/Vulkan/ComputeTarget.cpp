#include "flpch.h"
#include "ComputeTarget.h"

#include "Flourish/Backends/Vulkan/ComputePipeline.h"

namespace Flourish::Vulkan
{
    DescriptorSet* ComputeTarget::GetPipelineDescriptorSet(ComputePipeline* pipeline)
    {
        if (m_PipelineDescriptorSets.find(pipeline) == m_PipelineDescriptorSets.end())
            m_PipelineDescriptorSets[pipeline] = std::make_shared<DescriptorSet>(pipeline->GetDescriptorSetLayout());
        return m_PipelineDescriptorSets[pipeline].get();
    }
}