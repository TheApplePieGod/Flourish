#pragma once

#include "Flourish/Api/DescriptorSetAllocator.h"
#include "Flourish/Backends/Vulkan/Util/Common.h"

namespace Flourish::Vulkan
{
    class DescriptorPool;
    class DescriptorSetAllocator : public Flourish::DescriptorSetAllocator
    {
    public:
        DescriptorSetAllocator(const DescriptorSetAllocatorCreateInfo& createInfo);
        ~DescriptorSetAllocator() override;

        std::shared_ptr<Flourish::DescriptorSet> Allocate(const DescriptorSetCreateInfo& createInfo) override;

    private:
        std::shared_ptr<DescriptorPool> m_Pool;
    };
}
