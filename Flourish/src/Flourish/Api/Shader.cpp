#include "flpch.h"
#include "Shader.h"

#include "Flourish/Api/Context.h"
#include "Flourish/Backends/Vulkan/Shader.h"

namespace Flourish
{
    std::shared_ptr<Shader> Shader::Create(const ShaderCreateInfo& createInfo)
    {
        FL_ASSERT(Context::BackendType() != BackendType::None, "Must initialize Context before creating a Shader");

        try
        {
            switch (Context::BackendType())
            {
                case BackendType::Vulkan: { return std::make_shared<Vulkan::Shader>(createInfo); }
            }
        }
        catch (const std::exception& e)
            return nullptr;

        FL_ASSERT(false, "Shader not supported for backend");
        return nullptr;
    }
}