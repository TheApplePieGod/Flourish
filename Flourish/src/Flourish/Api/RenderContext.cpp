#include "flpch.h"
#include "RenderContext.h"

#include "Flourish/Api/Context.h"
#include "Flourish/Backends/Vulkan/RenderContext.h"

namespace Flourish
{
    std::shared_ptr<RenderContext> RenderContext::Create(const RenderContextCreateInfo& createInfo)
    {
        FL_ASSERT(Context::GetBackendType() != BackendType::None, "Must initialize Context before creating a RenderContext");

        switch (Context::GetBackendType())
        {
            case BackendType::None:
            { FL_ASSERT(false, "Must initialize Context before creating a RenderContext"); } return nullptr;
            case BackendType::Vulkan: { return std::make_shared<Vulkan::RenderContext>(createInfo); }
        }

        FL_ASSERT(false, "RenderContext not supported for backend");
        return nullptr;
    }
}