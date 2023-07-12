#include "flpch.h"
#include "RayTracingGroupTable.h"

#include "Flourish/Backends/Vulkan/RayTracing/RayTracingGroupTable.h"

namespace Flourish
{
    std::shared_ptr<RayTracingGroupTable> RayTracingGroupTable::Create(const RayTracingGroupTableCreateInfo& createInfo)
    {
        FL_ASSERT(Context::BackendType() != BackendType::None, "Must initialize Context before creating a RayTracingGroupTable");

        try
        {
            switch (Context::BackendType())
            {
                case BackendType::Vulkan: { return std::make_shared<Vulkan::RayTracingGroupTable>(createInfo); }
            }
        }
        catch (const std::exception& e) {}

        FL_ASSERT(false, "Failed to create RayTracingGroupTable");
        return nullptr;
    }
}
