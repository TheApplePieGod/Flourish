#include "flpch.h"
#include "RenderContext.h"

#include "Flourish/Api/Context.h"
#include "Flourish/Backends/Vulkan/RenderContext.h"

namespace Flourish
{
    std::shared_ptr<RenderContext> RenderContext::Create(const RenderContextCreateInfo& createInfo)
    {
        FL_ASSERT(Context::BackendType() != BackendType::None, "Must initialize Context before creating a RenderContext");

        try
        {
            switch (Context::BackendType())
            {
                case BackendType::Vulkan: { return std::make_shared<Vulkan::RenderContext>(createInfo); }
            }
        }
        catch (const std::exception& e) {}

        FL_ASSERT(false, "Failed to create RenderContext");
        return nullptr;
    }
}