#include "flpch.h"
#include "CommandBuffer.h"

#include "Flourish/Api/Context.h"
#include "Flourish/Backends/Vulkan/CommandBuffer.h"

namespace Flourish
{
    std::shared_ptr<CommandBuffer> CommandBuffer::Create(const CommandBufferCreateInfo& createInfo)
    {
        FL_ASSERT(Context::BackendType() != BackendType::None, "Must initialize Context before creating a CommandBuffer");

        try
        {
            switch (Context::BackendType())
            {
                case BackendType::Vulkan: { return std::make_shared<Vulkan::CommandBuffer>(createInfo); }
            }
        }
        catch (const std::exception& e)
            return nullptr;

        FL_ASSERT(false, "CommandBuffer not supported for backend");
        return nullptr;
    }
}