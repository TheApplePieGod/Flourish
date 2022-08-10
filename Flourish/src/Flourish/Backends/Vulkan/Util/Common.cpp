#include "flpch.h"
#include "Common.h"

namespace Flourish::Vulkan
{
    bool Common::SupportsExtension(const std::vector<VkExtensionProperties>& extensions, const char* extension)
    {
        return std::find_if(
            extensions.begin(),
            extensions.end(),
            [&extensions, extension](const VkExtensionProperties& arg)
            {
                return strcmp(arg.extensionName, extension);
            }
        ) != extensions.end();
    }
}