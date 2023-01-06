#include "flpch.h"
#include "ComputeTarget.h"

#include "Flourish/Api/Context.h"
#include "Flourish/Backends/Vulkan/ComputeTarget.h"

namespace Flourish
{
    std::shared_ptr<ComputeTarget> ComputeTarget::Create()
    {
        FL_ASSERT(Context::BackendType() != BackendType::None, "Must initialize Context before creating a ComputeTarget");

        try
        {
            switch (Context::BackendType())
            {
                case BackendType::Vulkan: { return std::make_shared<Vulkan::ComputeTarget>(); }
            }
        }
        catch (const std::exception& e) {}

        FL_ASSERT(false, "Failed to create ComputeTarget");
        return nullptr;
    }
}