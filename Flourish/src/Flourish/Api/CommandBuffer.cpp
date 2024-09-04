#include "flpch.h"
#include "CommandBuffer.h"

#include "Flourish/Api/Context.h"
#include "Flourish/Backends/Vulkan/CommandBuffer.h"

namespace Flourish
{
    CommandBuffer::CommandBuffer(const CommandBufferCreateInfo& createInfo)
            : m_Info(createInfo)
    {
        m_Id = Context::GetNextId();
    }

    std::shared_ptr<CommandBuffer> CommandBuffer::Create(const CommandBufferCreateInfo& createInfo)
    {
        FL_ASSERT(Context::BackendType() != BackendType::None, "Must initialize Context before creating a CommandBuffer");

        try
        {
            switch (Context::BackendType())
            {
                default: return nullptr;
                case BackendType::Vulkan: { return std::make_shared<Vulkan::CommandBuffer>(createInfo); }
            }
        }
        catch (const std::exception& e) {}

        FL_ASSERT(false, "Failed to create CommandBuffer");
        return nullptr;
    }
}
