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
        struct SetData
        {
            bool Exists = false;
            std::vector<ReflectionDataElement> ReflectionData;
            std::shared_ptr<DescriptorPool> Pool;
            u32 DynamicOffsetIndex = 0; // When computing dynamic offsets
            u32 DynamicOffsetCount = 0;
        };

    public:
        Shader(const ShaderCreateInfo& createInfo);
        ~Shader() override;

        std::shared_ptr<Flourish::DescriptorSet> CreateDescriptorSet(const DescriptorSetCreateInfo& createInfo) override;

        // TS
        VkPipelineShaderStageCreateInfo DefineShaderStage(const char* entrypoint = "main");

        // TS
        inline VkShaderModule GetShaderModule() const { return m_ShaderModule; }
        inline u32 GetTotalDynamicOffsets() const { return m_TotalDynamicOffsets; }
        inline const auto& GetSetData() const { return m_SetData; }
        inline bool DoesSetExist(u32 setIndex) const { return setIndex < m_SetData.size() && m_SetData[setIndex].Exists; }

    private:
        void Reflect(const std::vector<u32>& compiledData);

    private:
        std::vector<SetData> m_SetData;
        VkShaderModule m_ShaderModule = nullptr;
        u32 m_TotalDynamicOffsets = 0;
    };
}
