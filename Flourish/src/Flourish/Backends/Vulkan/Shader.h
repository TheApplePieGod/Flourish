#pragma once

#include "Flourish/Api/Shader.h"
#include "Flourish/Backends/Vulkan/Util/Common.h"

namespace Flourish::Vulkan
{
    struct ReflectionDataElement
    {
        ReflectionDataElement() = default;
        ReflectionDataElement(ShaderResourceType resourceType, ShaderType accessType, u32 bindingIndex, u32 setIndex, u32 size, u32 arrayCount)
            : ResourceType(resourceType), AccessType(accessType), BindingIndex(bindingIndex), SetIndex(setIndex), Size(size), ArrayCount(arrayCount)
        {}

        ShaderResourceType ResourceType;
        ShaderType AccessType = 0;
        u32 BindingIndex;
        u32 SetIndex;
        u32 Size = 0;
        u32 ArrayCount;
    };

    class DescriptorPool;
    class ResourceSet;
    class Shader : public Flourish::Shader
    {
    public:
        Shader(const ShaderCreateInfo& createInfo);
        ~Shader() override;

        // TS
        VkPipelineShaderStageCreateInfo DefineShaderStage(const char* entrypoint = "main");

        // TS
        inline VkShaderModule GetShaderModule() const { return m_ShaderModule; }
        inline const auto& GetReflectionData() const { return m_ReflectionData; }
        inline const auto& GetPushConstantReflection() const { return m_PushConstantReflection; }

    private:
        void Reflect(const std::vector<u32>& compiledData);

    private:
        VkShaderModule m_ShaderModule = nullptr;
        std::vector<ReflectionDataElement> m_ReflectionData;
        ReflectionDataElement m_PushConstantReflection;
    };
}
