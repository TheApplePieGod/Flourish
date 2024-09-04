#include "flpch.h"
#include "GraphicsPipeline.h"

#include "Flourish/Backends/Vulkan/Shader.h"
#include "Flourish/Backends/Vulkan/RenderPass.h"
#include "Flourish/Backends/Vulkan/Context.h"
#include "Flourish/Backends/Vulkan/ResourceSet.h"
#include "Flourish/Backends/Vulkan/Util/DescriptorPool.h"

namespace Flourish::Vulkan
{
    static VkVertexInputBindingDescription GenerateVertexBindingDescription(const BufferLayout& layout)
    {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = layout.GetCalculatedStride();
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        return bindingDescription;
    }

    static std::vector<VkVertexInputAttributeDescription> GenerateVertexAttributeDescriptions(const std::vector<BufferLayoutElement>& elements)
    {
        std::vector<VkVertexInputAttributeDescription> attributeDescriptions(elements.size());

        u32 offset = 0;
        for (u32 i = 0; i < elements.size(); i++)
        {
            attributeDescriptions[i].binding = 0;
            attributeDescriptions[i].location = static_cast<u32>(i);
            attributeDescriptions[i].format = Common::ConvertBufferDataType(elements[i].DataType);
            attributeDescriptions[i].offset = offset;
            offset += elements[i].CalculatedSize;
        }
        
        return attributeDescriptions;
    }

    GraphicsPipeline::GraphicsPipeline(const GraphicsPipelineCreateInfo& createInfo, RenderPass* renderPass)
        : Flourish::GraphicsPipeline(createInfo)
    {
        m_RenderPass = renderPass;
        Recreate();
    }

    GraphicsPipeline::~GraphicsPipeline()
    {
        Cleanup();
    }

    void GraphicsPipeline::Recreate()
    {
        if (m_Created)
            FL_LOG_DEBUG("Recreating graphics pipeline");

        auto vertShader = static_cast<Shader*>(m_Info.VertexShader.Shader.get());
        auto fragShader = static_cast<Shader*>(m_Info.FragmentShader.Shader.get());
        VkPipelineShaderStageCreateInfo shaderStages[] = {
            vertShader->DefineShaderStage(),
            fragShader->DefineShaderStage()
        };

        // Update revision count immediately to ensure we don't continually try
        // and recreate if this revision fails
        m_ShaderRevisions = { vertShader->GetRevisionCount(), fragShader->GetRevisionCount() };

        // Populate descriptor data
        PipelineDescriptorData newDescData;
        std::array<Shader*, 2> shaders = { vertShader, fragShader };
        newDescData.Populate(shaders.data(), shaders.size(), m_Info.AccessOverrides);
        newDescData.Compatability = ResourceSetPipelineCompatabilityFlags::Graphics;

        if (m_Created && !(newDescData == m_DescriptorData))
        {
            FL_LOG_ERROR("Cannot recreate graphics pipeline because new shader bindings are different or incompatible");
            return;
        }

        if (m_Created)
            Cleanup();

        m_DescriptorData = std::move(newDescData);

        // Populate specialization constants
        std::array<std::vector<SpecializationConstant>, 2> specs = { m_Info.VertexShader.Specializations, m_Info.FragmentShader.Specializations };
        PipelineSpecializationHelper specHelper;
        specHelper.Populate(shaders.data(), specs.data(), shaders.size());
        shaderStages[0].pSpecializationInfo = &specHelper.SpecInfos[0];
        shaderStages[1].pSpecializationInfo = &specHelper.SpecInfos[1];

        auto bindingDescription = GenerateVertexBindingDescription(m_Info.VertexLayout);
        auto attributeDescriptions = GenerateVertexAttributeDescriptions(m_Info.VertexLayout.GetElements());

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = m_Info.VertexInput ? 1 : 0;
        vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
        vertexInputInfo.vertexAttributeDescriptionCount = m_Info.VertexInput ? static_cast<u32>(attributeDescriptions.size()) : 0;
        vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();
        
        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = m_Info.VertexInput ? Common::ConvertVertexTopology(m_Info.VertexTopology) : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = 0.0f;
        viewport.height = 0.0f;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor{};
        scissor.offset = { 0, 0 };
        scissor.extent = { 0, 0 };

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.pViewports = &viewport;
        viewportState.scissorCount = 1;
        viewportState.pScissors = &scissor;

        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = Common::ConvertCullMode(m_Info.CullMode);
        rasterizer.frontFace = Common::ConvertWindingOrder(m_Info.WindingOrder);
        rasterizer.depthBiasEnable = VK_FALSE;
        rasterizer.depthBiasConstantFactor = 0.0f;
        rasterizer.depthBiasClamp = 0.0f;
        rasterizer.depthBiasSlopeFactor = 0.0f;

        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = m_RenderPass->GetConvertedSampleCount();
        multisampling.minSampleShading = 1.0f;
        multisampling.pSampleMask = nullptr;
        multisampling.alphaToCoverageEnable = VK_FALSE;
        multisampling.alphaToOneEnable = VK_FALSE;

        std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments(m_Info.BlendStates.size());
        for (u32 i = 0; i < m_Info.BlendStates.size(); i++)
        {
            colorBlendAttachments[i].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            colorBlendAttachments[i].blendEnable = m_Info.BlendStates[i].BlendEnable;
            colorBlendAttachments[i].srcColorBlendFactor = Common::ConvertBlendFactor(m_Info.BlendStates[i].SrcColorBlendFactor);
            colorBlendAttachments[i].dstColorBlendFactor = Common::ConvertBlendFactor(m_Info.BlendStates[i].DstColorBlendFactor);
            colorBlendAttachments[i].colorBlendOp = Common::ConvertBlendOperation(m_Info.BlendStates[i].ColorBlendOperation);
            colorBlendAttachments[i].srcAlphaBlendFactor = Common::ConvertBlendFactor(m_Info.BlendStates[i].SrcAlphaBlendFactor);
            colorBlendAttachments[i].dstAlphaBlendFactor = Common::ConvertBlendFactor(m_Info.BlendStates[i].DstAlphaBlendFactor);
            colorBlendAttachments[i].alphaBlendOp = Common::ConvertBlendOperation(m_Info.BlendStates[i].AlphaBlendOperation);
        }

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.attachmentCount = static_cast<u32>(colorBlendAttachments.size());
        colorBlending.pAttachments = colorBlendAttachments.data();
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.logicOp = VK_LOGIC_OP_COPY;
        colorBlending.blendConstants[0] = 0.0f;
        colorBlending.blendConstants[1] = 0.0f;
        colorBlending.blendConstants[2] = 0.0f;
        colorBlending.blendConstants[3] = 0.0f;

        VkDynamicState dynamicStates[] = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
            VK_DYNAMIC_STATE_LINE_WIDTH
        };
        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = 3;
        dynamicState.pDynamicStates = dynamicStates;

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
        ), "GraphicsPipeline create layout"))
            throw std::exception();

        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable = m_Info.DepthConfig.DepthTest;
        depthStencil.depthWriteEnable = m_Info.DepthConfig.DepthWrite;
        depthStencil.depthCompareOp = Common::ConvertDepthComparison(m_Info.DepthConfig.CompareOperation);
        depthStencil.depthBoundsTestEnable = VK_FALSE;
        depthStencil.minDepthBounds = 0.0f;
        depthStencil.maxDepthBounds = 1.0f;
        depthStencil.stencilTestEnable = VK_FALSE;
        depthStencil.front = {};
        depthStencil.back = {};

        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.flags = VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = m_PipelineLayout;
        pipelineInfo.renderPass = m_RenderPass->GetRenderPass();
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
        pipelineInfo.basePipelineIndex = -1;

        bool fillCompatible = m_Info.CompatibleSubpasses.empty();
        u32 subpassCount = fillCompatible ? m_RenderPass->GetSubpasses().size() : m_Info.CompatibleSubpasses.size();
        for (u32 i = 0; i < subpassCount; i++)
        {
            if (fillCompatible)
                m_Info.CompatibleSubpasses.push_back(i);
            pipelineInfo.subpass = m_Info.CompatibleSubpasses[i];

            // Ensure Compatability
            if (m_RenderPass->GetColorAttachmentCount(m_Info.CompatibleSubpasses[i]) != m_Info.BlendStates.size())
            {
                FL_LOG_ERROR("Pipeline has blend state count that does not match with a compatible subpass");
                throw std::exception();
            }
            
            if (i > 0)
            {
                pipelineInfo.flags = VK_PIPELINE_CREATE_DERIVATIVE_BIT;
                pipelineInfo.basePipelineHandle = m_Pipelines[m_Info.CompatibleSubpasses[0]];
            }

            VkPipeline pipeline;
            if(!FL_VK_CHECK_RESULT(vkCreateGraphicsPipelines(
                Context::Devices().Device(),
                VK_NULL_HANDLE,
                1,
                &pipelineInfo,
                nullptr,
                &pipeline
            ), "GraphicsPipeline create pipeline"))
                throw std::exception();

            m_Pipelines[m_Info.CompatibleSubpasses[i]] = pipeline;
        }

        m_Created = true;
    }

    void GraphicsPipeline::Cleanup()
    {
        auto pipelines = m_Pipelines;
        auto layout = m_PipelineLayout;
        Context::FinalizerQueue().Push([=]()
        {
            for (auto& pair : pipelines)
                vkDestroyPipeline(Context::Devices().Device(), pair.second, nullptr);
            if (layout)
                vkDestroyPipelineLayout(Context::Devices().Device(), layout, nullptr);
        }, "Graphics pipeline free");

        m_Pipelines.clear();
        m_PipelineLayout = VK_NULL_HANDLE;
    }

    bool GraphicsPipeline::ValidateShaders()
    {
        auto vertShader = static_cast<Shader*>(m_Info.VertexShader.Shader.get());
        auto fragShader = static_cast<Shader*>(m_Info.FragmentShader.Shader.get());
        if (vertShader->GetRevisionCount() != m_ShaderRevisions[0] ||
            fragShader->GetRevisionCount() != m_ShaderRevisions[1])
        {
            Recreate();
            return true;
        }
        
        return false;
    }

    std::shared_ptr<Flourish::ResourceSet> GraphicsPipeline::CreateResourceSet(u32 setIndex, const ResourceSetCreateInfo& createInfo)
    {
        return m_DescriptorData.CreateResourceSet(
            setIndex,
            ResourceSetPipelineCompatabilityFlags::Graphics,
            createInfo
        );
    }
}
