#include "flpch.h"
#include "ResourceSetAllocator.h"

#include "Flourish/Backends/Vulkan/ResourceSetAllocator.h"

namespace Flourish
{
    ResourceSetAllocator::ResourceSetAllocator(const ResourceSetAllocatorCreateInfo& createInfo)
        : m_Info(createInfo)
    {}

    std::shared_ptr<ResourceSetAllocator> ResourceSetAllocator::Create(const ResourceSetAllocatorCreateInfo& createInfo)
    {
        FL_ASSERT(Context::BackendType() != BackendType::None, "Must initialize Context before creating a ResourceSetAllocator");

        try
        {
            switch (Context::BackendType())
            {
                default: return nullptr;
                case BackendType::Vulkan: { return std::make_shared<Vulkan::ResourceSetAllocator>(createInfo); }
            }
        }
        catch (const std::exception& e) {}

        FL_ASSERT(false, "Failed to create ResourceSetAllocator");
        return nullptr;
    }
}
