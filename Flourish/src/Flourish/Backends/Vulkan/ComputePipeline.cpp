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
        Recreate();
    }

    ComputePipeline::~ComputePipeline()
    {
        Cleanup();
    }

    void ComputePipeline::Recreate()
    {
        if (m_Created)
            FL_LOG_DEBUG("Recreating compute pipeline");

        auto shader = static_cast<Shader*>(m_Info.Shader.Shader.get());
        VkPipelineShaderStageCreateInfo shaderStage = shader->DefineShaderStage();

        // Update revision count immediately to ensure we don't continually try
        // and recreate if this revision fails
        m_ShaderRevision = shader->GetRevisionCount();

        // Populate descriptor data
        PipelineDescriptorData newDescData;
        newDescData.Populate(&shader, 1, m_Info.AccessOverrides);
        newDescData.Compatability = ResourceSetPipelineCompatabilityFlags::Compute;

        if (m_Created && !(newDescData == m_DescriptorData))
        {
            FL_LOG_ERROR("Cannot recreate compute pipeline because new shader bindings are different or incompatible");
            return;
        }

        if (m_Created)
            Cleanup();

        m_DescriptorData = std::move(newDescData);

        // Populate specialization constants
        PipelineSpecializationHelper specHelper;
        specHelper.Populate(&shader, &m_Info.Shader.Specializations, 1);
        shaderStage.pSpecializationInfo = &specHelper.SpecInfos[0];

        u32 setCount = m_DescriptorData.SetData.size();
        std::vector<VkDescriptorSetLayout> layouts(setCount, VK_NULL_HANDLE);
        for (u32 i = 0; i < setCount; i++)
        {
            if (m_DescriptorData.SetData[i].Exists)
                layouts[i] = m_DescriptorData.SetData[i].Pool->GetLayout();
            else
                layouts[i] = m_DescriptorData.EmptySetLayout;
        }

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

        m_Created = true;
    }

    void ComputePipeline::Cleanup()
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

        m_Pipeline = VK_NULL_HANDLE;
        m_PipelineLayout = VK_NULL_HANDLE;
    }

    bool ComputePipeline::ValidateShaders()
    {
        auto shader = static_cast<Shader*>(m_Info.Shader.Shader.get());
        if (shader->GetRevisionCount() != m_ShaderRevision)
        {
            Recreate();
            return true;
        }
        
        return false;
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
