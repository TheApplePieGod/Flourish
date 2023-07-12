#include "flpch.h"
#include "RayTracingPipeline.h"

#include "Flourish/Api/Context.h"
#include "Flourish/Backends/Vulkan/RayTracing/RayTracingPipeline.h"

namespace Flourish
{
    std::shared_ptr<RayTracingPipeline> RayTracingPipeline::Create(const RayTracingPipelineCreateInfo& createInfo)
    {
        FL_ASSERT(Context::BackendType() != BackendType::None, "Must initialize Context before creating a RayTracingPipeline");

        try
        {
            switch (Context::BackendType())
            {
                case BackendType::Vulkan: { return std::make_shared<Vulkan::RayTracingPipeline>(createInfo); }
            }
        }
        catch (const std::exception& e) {}

        FL_ASSERT(false, "Failed to create RayTracingPipeline");
        return nullptr;
    }
}
