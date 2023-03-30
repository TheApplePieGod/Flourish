#pragma once

#include "Flourish/Api/Shader.h"
#include "Flourish/Backends/Vulkan/Util/Common.h"

namespace Flourish::Vulkan
{
    class DescriptorPool;
    class Shader : public Flourish::Shader
    {
    public:
        Shader(const ShaderCreateInfo& createInfo);
        ~Shader() override;

        // TS
        std::shared_ptr<Flourish::DescriptorSet> CreateDescriptorSet(const DescriptorSetCreateInfo& createInfo) override;

        // TS
        VkPipelineShaderStageCreateInfo DefineShaderStage(const char* entrypoint = "main");

        // TS
        inline VkShaderModule GetShaderModule() const { return m_ShaderModule; }

    private:
        void Reflect(const std::vector<u32>& compiledData);

    private:
        VkShaderModule m_ShaderModule = nullptr;
        std::mutex m_DescriptorPoolMutex;
        std::vector<std::shared_ptr<DescriptorPool>> m_SetPools;
    };
}
