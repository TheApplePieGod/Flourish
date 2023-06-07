#include "flpch.h"
#include "DescriptorSetAllocator.h"

#include "Flourish/Backends/Vulkan/DescriptorSetAllocator.h"

namespace Flourish
{
    std::shared_ptr<DescriptorSetAllocator> DescriptorSetAllocator::Create(const DescriptorSetAllocatorCreateInfo& createInfo)
    {
        FL_ASSERT(Context::BackendType() != BackendType::None, "Must initialize Context before creating a DescriptorSetAllocator");

        try
        {
            switch (Context::BackendType())
            {
                case BackendType::Vulkan: { return std::make_shared<Vulkan::DescriptorSetAllocator>(createInfo); }
            }
        }
        catch (const std::exception& e) {}

        FL_ASSERT(false, "Failed to create DescriptorSetAllocator");
        return nullptr;
    }
}
