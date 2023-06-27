#include "flpch.h"
#include "ComputePipeline.h"

#include "Flourish/Backends/Vulkan/Shader.h"
#include "Flourish/Backends/Vulkan/Context.h"
#include "Flourish/Backends/Vulkan/ResourceSet.h"
#include "Flourish/Backends/Vulkan/Util/DescriptorPool.h"

namespace Flourish::Vulkan
{
    ComputePipeline::ComputePipeline(const ComputePipelineCreateInfo& createInfo)
        : Flourish::ComputePipeline(createInfo)
    {
        auto shader = static_cast<Shader*>(createInfo.ComputeShader.get());
        VkPipelineShaderStageCreateInfo shaderStage = shader->DefineShaderStage();

        m_DescriptorData.Populate(&shader, 1, m_Info.AccessOverrides);
        m_DescriptorData.Compatability = ResourceSetPipelineCompatabilityFlags::Compute;

        u32 setCount = m_DescriptorData.SetData.size();
        std::vector<VkDescriptorSetLayout> layouts(setCount, VK_NULL_HANDLE);
        for (u32 i = 0; i < setCount; i++)
            if (m_DescriptorData.SetData[i].Exists)
                layouts[i] = m_DescriptorData.SetData[i].Pool->GetLayout();

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = setCount;
        pipelineLayoutInfo.pSetLayouts = layouts.data();
        if (m_DescriptorData.PushConstantRange.size > 0)
        {
            pipelineLayoutInfo.pushConstantRangeCount = 1;
            pipelineLayoutInfo.pPushConstantRanges = &m_DescriptorData.PushConstantRange;
        }
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

    std::shared_ptr<Flourish::ResourceSet> ComputePipeline::CreateResourceSet(u32 setIndex, const ResourceSetCreateInfo& createInfo)
    {
        return m_DescriptorData.CreateResourceSet(
            setIndex,
            ResourceSetPipelineCompatabilityFlags::Compute,
            createInfo
        );
    }
}
