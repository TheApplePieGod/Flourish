#pragma once

#include "Flourish/Api/ComputeTarget.h"
#include "Flourish/Backends/Vulkan/Util/Common.h"
#include "Flourish/Backends/Vulkan/Util/DescriptorSet.h"

namespace Flourish::Vulkan
{
    class ComputePipeline;
    class ComputeTarget : public Flourish::ComputeTarget
    {
    public:
        ComputeTarget() = default;
        
        DescriptorSet* GetPipelineDescriptorSet(ComputePipeline* pipeline);

    private:
        std::unordered_map<ComputePipeline*, std::shared_ptr<DescriptorSet>> m_PipelineDescriptorSets;
    };
}