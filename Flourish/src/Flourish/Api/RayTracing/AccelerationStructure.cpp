#include "flpch.h"
#include "AccelerationStructure.h"

#include "Flourish/Backends/Vulkan/RayTracing/AccelerationStructure.h"

namespace Flourish
{
    std::shared_ptr<AccelerationStructure> AccelerationStructure::Create(const AccelerationStructureCreateInfo& createInfo)
    {
        FL_ASSERT(Context::BackendType() != BackendType::None, "Must initialize Context before creating a AccelerationStructure");

        try
        {
            switch (Context::BackendType())
            {
                default: return nullptr;
                case BackendType::Vulkan: { return std::make_shared<Vulkan::AccelerationStructure>(createInfo); }
            }
        }
        catch (const std::exception& e) {}

        FL_ASSERT(false, "Failed to create AccelerationStructure");
        return nullptr;
    }
}
