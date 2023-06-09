#pragma once

#include "Flourish/Api/Shader.h"
#include "Flourish/Api/DescriptorSet.h"

namespace Flourish
{
    // Access flags MUST match the shader usage
    struct DescriptorSetLayoutElement
    {
        u32 BindingIndex = 0;
        ShaderResourceType ResourceType;
        ShaderType AccessFlags = 0;
        u32 ArrayCount = 1;
    };

    struct DescriptorSetAllocatorCreateInfo
    {
        std::vector<DescriptorSetLayoutElement> Layout;
        DescriptorSetPipelineCompatability Compatability;
    };

    class DescriptorSetAllocator
    {
    public:
        DescriptorSetAllocator(const DescriptorSetAllocatorCreateInfo& createInfo)
            : m_Info(createInfo)
        {}
        virtual ~DescriptorSetAllocator() = default;

        // TS
        virtual std::shared_ptr<DescriptorSet> Allocate(const DescriptorSetCreateInfo& createInfo) = 0;

    public:
        static std::shared_ptr<DescriptorSetAllocator> Create(const DescriptorSetAllocatorCreateInfo& createInfo);

    protected:
        DescriptorSetAllocatorCreateInfo m_Info;
    };
}
