#pragma once

#include "Flourish/Api/ResourceSetAllocator.h"
#include "Flourish/Backends/Vulkan/Util/Common.h"

namespace Flourish::Vulkan
{
    class DescriptorPool;
    class ResourceSetAllocator : public Flourish::ResourceSetAllocator
    {
    public:
        ResourceSetAllocator(const ResourceSetAllocatorCreateInfo& createInfo);
        ~ResourceSetAllocator() override;

        std::shared_ptr<Flourish::ResourceSet> Allocate(const ResourceSetCreateInfo& createInfo) override;

    private:
        std::shared_ptr<DescriptorPool> m_Pool;
    };
}
