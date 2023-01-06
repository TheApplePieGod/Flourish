#include "flpch.h"
#include "Framebuffer.h"

#include "Flourish/Api/Context.h"
#include "Flourish/Backends/Vulkan/Framebuffer.h"

namespace Flourish
{
    std::shared_ptr<Framebuffer> Framebuffer::Create(const FramebufferCreateInfo& createInfo)
    {
        FL_ASSERT(Context::BackendType() != BackendType::None, "Must initialize Context before creating a Framebuffer");

        try
        {
            switch (Context::BackendType())
            {
                case BackendType::Vulkan: { return std::make_shared<Vulkan::Framebuffer>(createInfo); }
            }
        }
        catch (const std::exception& e)
            return nullptr;

        FL_ASSERT(false, "Framebuffer not supported for backend");
        return nullptr;
    }
}