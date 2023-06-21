#include "flpch.h"
#include "RayTracingPipeline.h"

#include "Flourish/Backends/Vulkan/Shader.h"
#include "Flourish/Backends/Vulkan/Context.h"
#include "Flourish/Backends/Vulkan/ResourceSet.h"
#include "Flourish/Backends/Vulkan/Util/DescriptorPool.h"

namespace Flourish::Vulkan
{
    RayTracingPipeline::RayTracingPipeline(const RayTracingPipelineCreateInfo& createInfo)
        : Flourish::RayTracingPipeline(createInfo)
    {
        std::vector<VkPipelineShaderStageCreateInfo> stages;
        std::vector<Shader*> shaders;
        std::vector<VkRayTracingShaderGroupCreateInfoKHR> groups;

        for (auto& _shader : m_Info.Shaders)
        {
            auto shader = static_cast<Shader*>(_shader.get());
            stages.emplace_back(shader->DefineShaderStage());
            shaders.emplace_back(shader);
        };

        for (auto& group : m_Info.Groups)
        {
            groups.emplace_back();
            groups.back().sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;

            if (group.GeneralShader != -1)
            {
                FL_ASSERT(
                    (group.AnyHitShader == -1) && (group.ClosestHitShader == -1) && (group.IntersectionShader == -1),
                    "General shaders can only exist on their own"
                );
                FL_ASSERT(
                    shaders[group.GeneralShader]->GetType() == ShaderTypeFlags::RayGen ||
                    shaders[group.GeneralShader]->GetType() == ShaderTypeFlags::RayMiss,
                    "General shaders can only be RayGen or RayMiss shader types"
                );

                groups.back().type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
                groups.back().generalShader = group.GeneralShader;
                groups.back().closestHitShader = VK_SHADER_UNUSED_KHR;
                groups.back().anyHitShader = VK_SHADER_UNUSED_KHR;
                groups.back().intersectionShader = VK_SHADER_UNUSED_KHR;
                continue;
            }

            if (group.IntersectionShader != -1)
                groups.back().type = VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR;
            else
                groups.back().type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;

            groups.back().intersectionShader = group.IntersectionShader == -1 ? VK_SHADER_UNUSED_KHR : group.IntersectionShader;
            groups.back().anyHitShader = group.AnyHitShader == -1 ? VK_SHADER_UNUSED_KHR : group.AnyHitShader;
            groups.back().closestHitShader = group.ClosestHitShader == -1 ? VK_SHADER_UNUSED_KHR : group.ClosestHitShader;
        }

        m_DescriptorData.Populate(shaders.data(), shaders.size());
        m_DescriptorData.Compatability = ResourceSetPipelineCompatabilityFlags::RayTracing;

        u32 setCount = m_DescriptorData.SetData.size();
        std::vector<VkDescriptorSetLayout> layouts(setCount, VK_NULL_HANDLE);
        for (u32 i = 0; i < setCount; i++)
            if (m_DescriptorData.SetData[i].Exists)
                layouts[i] = m_DescriptorData.SetData[i].Pool->GetLayout();

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = setCount;
        pipelineLayoutInfo.pSetLayouts = layouts.data();
        pipelineLayoutInfo.pushConstantRangeCount = 0;
        pipelineLayoutInfo.pPushConstantRanges = nullptr;
        if (!FL_VK_CHECK_RESULT(vkCreatePipelineLayout(
            Context::Devices().Device(),
            &pipelineLayoutInfo,
            nullptr,
            &m_PipelineLayout
        ), "RayTracingPipeline create layout"))
            throw std::exception();

        VkRayTracingPipelineCreateInfoKHR pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
        pipelineInfo.layout = m_PipelineLayout;
        pipelineInfo.stageCount = stages.size();
        pipelineInfo.pStages = stages.data();
        pipelineInfo.groupCount = groups.size();
        pipelineInfo.pGroups = groups.data();
        pipelineInfo.maxPipelineRayRecursionDepth = std::max(1U, std::min(
            m_Info.MaxRayRecursionDepth,
            Context::Devices().RayTracingProperties().maxRayRecursionDepth
        ));
        if (!FL_VK_CHECK_RESULT(vkCreateRayTracingPipelinesKHR(
            Context::Devices().Device(),
            VK_NULL_HANDLE,
            VK_NULL_HANDLE,
            1, &pipelineInfo,
            nullptr,
            &m_Pipeline
        ), "RayTracingPipeline create pipeline"))
            throw std::exception();

        u32 handleDataSize = Context::Devices().RayTracingProperties().shaderGroupHandleSize * m_Info.Groups.size();
        m_GroupHandles.resize(handleDataSize);
        if (!FL_VK_CHECK_RESULT(vkGetRayTracingShaderGroupHandlesKHR(
            Context::Devices().Device(),
            m_Pipeline,
            0, m_Info.Groups.size(),
            handleDataSize,
            m_GroupHandles.data()
        ), "RayTracingPipeline get group handles"))
            throw std::exception();
    }

    RayTracingPipeline::~RayTracingPipeline()
    {
        auto pipeline = m_Pipeline;
        auto layout = m_PipelineLayout;
        Context::FinalizerQueue().Push([=]()
        {
            if (pipeline)
                vkDestroyPipeline(Context::Devices().Device(), pipeline, nullptr);
            if (layout)
                vkDestroyPipelineLayout(Context::Devices().Device(), layout, nullptr);
        }, "RayTracing pipeline free");
    }

    std::shared_ptr<Flourish::ResourceSet> RayTracingPipeline::CreateResourceSet(u32 setIndex, const ResourceSetCreateInfo& createInfo)
    {
        return m_DescriptorData.CreateResourceSet(
            setIndex,
            ResourceSetPipelineCompatabilityFlags::RayTracing,
            createInfo
        );
    }

    u8* RayTracingPipeline::GetGroupHandle(u32 groupIndex)
    {
        return m_GroupHandles.data() + (groupIndex * Context::Devices().RayTracingProperties().shaderGroupHandleSize);
    }
}
