#include "flpch.h"
#include "ComputePipeline.h"

#include "Flourish/Backends/Vulkan/Shader.h"
#include "Flourish/Backends/Vulkan/Util/Context.h"

namespace Flourish::Vulkan
{
    ComputePipeline::ComputePipeline(const ComputePipelineCreateInfo& createInfo)
        : Flourish::ComputePipeline(createInfo)
    {
        m_DescriptorSetLayout.Initialize(m_ProgramReflectionData);
        m_DescriptorSet = std::make_shared<DescriptorSet>(m_DescriptorSetLayout);

        VkPipelineShaderStageCreateInfo shaderStage = static_cast<Shader*>(createInfo.ComputeShader.get())->DefineShaderStage();

        VkDescriptorSetLayout layout[1] = { m_DescriptorSetLayout.GetLayout() };
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = layout;
        pipelineLayoutInfo.pushConstantRangeCount = 0;
        pipelineLayoutInfo.pPushConstantRanges = nullptr;
        FL_VK_ENSURE_RESULT(vkCreatePipelineLayout(Context::Devices().Device(), &pipelineLayoutInfo, nullptr, &m_PipelineLayout));

        VkComputePipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipelineInfo.layout = m_PipelineLayout;
        pipelineInfo.stage = shaderStage;
        FL_VK_ENSURE_RESULT(vkCreateComputePipelines(Context::Devices().Device(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_Pipeline));
    }

    ComputePipeline::~ComputePipeline()
    {
        m_DescriptorSetLayout.Shutdown();

        auto pipeline = m_Pipeline;
        auto layout = m_PipelineLayout;
        Context::DeleteQueue().Push([=]()
        {
            vkDestroyPipeline(Context::Devices().Device(), pipeline, nullptr);
            vkDestroyPipelineLayout(Context::Devices().Device(), layout, nullptr);
        });
    }
}