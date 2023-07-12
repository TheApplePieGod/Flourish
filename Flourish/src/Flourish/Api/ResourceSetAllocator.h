#pragma once

#include "Flourish/Api/Shader.h"
#include "Flourish/Api/ResourceSet.h"

namespace Flourish
{
    // Access flags MUST match the shader usage
    struct ResourceSetLayoutElement
    {
        u32 BindingIndex = 0;
        ShaderResourceType ResourceType;
        ShaderType AccessFlags = 0;
        u32 ArrayCount = 1;
    };

    struct ResourceSetAllocatorCreateInfo
    {
        std::vector<ResourceSetLayoutElement> Layout;
        ResourceSetPipelineCompatability Compatability;
    };

    class ResourceSetAllocator
    {
    public:
        ResourceSetAllocator(const ResourceSetAllocatorCreateInfo& createInfo);
        virtual ~ResourceSetAllocator() = default;

        // TS
        virtual std::shared_ptr<ResourceSet> Allocate(const ResourceSetCreateInfo& createInfo) = 0;

    public:
        static std::shared_ptr<ResourceSetAllocator> Create(const ResourceSetAllocatorCreateInfo& createInfo);

    protected:
        ResourceSetAllocatorCreateInfo m_Info;
    };
}
