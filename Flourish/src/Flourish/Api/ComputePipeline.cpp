#include "flpch.h"
#include "ComputePipeline.h"

#include "Flourish/Api/Context.h"
#include "Flourish/Backends/Vulkan/ComputePipeline.h"

namespace Flourish
{
    std::shared_ptr<ComputePipeline> ComputePipeline::Create(const ComputePipelineCreateInfo& createInfo)
    {
        FL_ASSERT(Context::BackendType() != BackendType::None, "Must initialize Context before creating a ComputePipeline");

        try
        {
            switch (Context::BackendType())
            {
                default: return nullptr;
                case BackendType::Vulkan: { return std::make_shared<Vulkan::ComputePipeline>(createInfo); }
            }
        }
        catch (const std::exception& e) {}

        FL_ASSERT(false, "Failed to create ComputePipeline");
        return nullptr;
    }
}
