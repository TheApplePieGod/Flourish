#include "flpch.h"
#include "ComputePipeline.h"

#include "Flourish/Backends/Vulkan/Shader.h"
#include "Flourish/Backends/Vulkan/Context.h"
#include "Flourish/Backends/Vulkan/Util/DescriptorPool.h"

namespace Flourish::Vulkan
{
    ComputePipeline::ComputePipeline(const ComputePipelineCreateInfo& createInfo)
        : Flourish::ComputePipeline(createInfo)
    {
        auto shader = static_cast<Shader*>(createInfo.ComputeShader.get());
        VkPipelineShaderStageCreateInfo shaderStage = shader->DefineShaderStage();

        auto& setData = shader->GetSetData();
        std::vector<VkDescriptorSetLayout> layouts(setData.size(), VK_NULL_HANDLE);
        for (u32 i = 0; i < setData.size(); i++)
        {
            if (setData[i].Exists)
                layouts[i] = setData[i].Pool->GetLayout();
        }

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = setData.size();
        pipelineLayoutInfo.pSetLayouts = layouts.data();
        pipelineLayoutInfo.pushConstantRangeCount = 0;
        pipelineLayoutInfo.pPushConstantRanges = nullptr;
        if (!FL_VK_CHECK_RESULT(vkCreatePipelineLayout(
            Context::Devices().Device(),
            &pipelineLayoutInfo,
            nullptr,
            &m_PipelineLayout
        ), "ComputePipeline create layout"))
            throw std::exception();

        VkComputePipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipelineInfo.layout = m_PipelineLayout;
        pipelineInfo.stage = shaderStage;
        if (!FL_VK_CHECK_RESULT(vkCreateComputePipelines(
            Context::Devices().Device(),
            VK_NULL_HANDLE,
            1, &pipelineInfo,
            nullptr,
            &m_Pipeline
        ), "ComputePipeline create pipeline"))
            throw std::exception();
    }

    ComputePipeline::~ComputePipeline()
    {
        auto pipeline = m_Pipeline;
        auto layout = m_PipelineLayout;
        Context::FinalizerQueue().Push([=]()
        {
            if (pipeline)
                vkDestroyPipeline(Context::Devices().Device(), pipeline, nullptr);
            if (layout)
                vkDestroyPipelineLayout(Context::Devices().Device(), layout, nullptr);
        }, "Compute pipeline free");
    }
}
