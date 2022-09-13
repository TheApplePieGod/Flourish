#include "flpch.h"
#include "RenderCommandEncoder.h"

#include "Flourish/Api/Context.h"
#include "Flourish/Backends/Vulkan/RenderCommandEncoder.h"

namespace Flourish
{
    std::shared_ptr<RenderCommandEncoder> RenderCommandEncoder::Create(const RenderCommandEncoderCreateInfo& createInfo)
    {
        FL_ASSERT(Context::BackendType() != BackendType::None, "Must initialize Context before creating a RenderCommandEncoder");

        switch (Context::BackendType())
        {
            case BackendType::Vulkan: { return std::make_shared<Vulkan::RenderCommandEncoder>(createInfo); }
        }

        FL_ASSERT(false, "RenderCommandEncoder not supported for backend");
        return nullptr;
    }
}