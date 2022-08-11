#include "flpch.h"
#include "Buffer.h"

#include "Flourish/Api/Context.h"
#include "Flourish/Backends/Vulkan/Buffer.h"

namespace Flourish
{
    std::shared_ptr<Buffer> Buffer::Create(const BufferCreateInfo& createInfo)
    {
        FL_ASSERT(Context::GetBackendType() != BackendType::None, "Must initialize Context before creating a Buffer");

        switch (Context::GetBackendType())
        {
            case BackendType::Vulkan: { return std::make_shared<Vulkan::Buffer>(createInfo); }
        }

        FL_ASSERT(false, "Buffer not supported for backend");
        return nullptr;
    }
}