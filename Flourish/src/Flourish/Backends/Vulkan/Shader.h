#pragma once

#include "Flourish/Api/Shader.h"
#include "Flourish/Backends/Vulkan/Util/Common.h"

namespace Flourish::Vulkan
{
    class DescriptorPool;
    class DescriptorSet;
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

    private:
        void Reflect(const std::vector<u32>& compiledData);

    private:
        VkShaderModule m_ShaderModule = nullptr;
        std::vector<ReflectionDataElement> m_ReflectionData;
    };
}
