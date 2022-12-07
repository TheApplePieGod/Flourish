#include "flpch.h"
#include "GraphicsPipeline.h"

#include "Flourish/Backends/Vulkan/Shader.h"
#include "Flourish/Backends/Vulkan/RenderPass.h"
#include "Flourish/Backends/Vulkan/Util/Context.h"

namespace Flourish::Vulkan
{
    static VkVertexInputBindingDescription GenerateVertexBindingDescription(const BufferLayout& layout)
    {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = layout.GetStride();
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

    GraphicsPipeline::GraphicsPipeline(const GraphicsPipelineCreateInfo& createInfo, RenderPass* renderPass, VkSampleCountFlagBits sampleCount)
        : Flourish::GraphicsPipeline(createInfo)
    {
        m_DescriptorSetLayout.Initialize(m_ProgramReflectionData);

        VkPipelineShaderStageCreateInfo shaderStages[] = {
            static_cast<Shader*>(createInfo.VertexShader.get())->DefineShaderStage(),
            static_cast<Shader*>(createInfo.FragmentShader.get())->DefineShaderStage()
        };

        auto bindingDescription = GenerateVertexBindingDescription(createInfo.VertexLayout);
        auto attributeDescriptions = GenerateVertexAttributeDescriptions(createInfo.VertexLayout.GetElements());

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = createInfo.VertexInput ? 1 : 0;
        vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
        vertexInputInfo.vertexAttributeDescriptionCount = createInfo.VertexInput ? static_cast<u32>(attributeDescriptions.size()) : 0;
        vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();
        
        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = Common::ConvertVertexTopology(createInfo.VertexTopology);
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
        rasterizer.cullMode = Common::ConvertCullMode(createInfo.CullMode);
        rasterizer.frontFace = Common::ConvertWindingOrder(createInfo.WindingOrder);
        rasterizer.depthBiasEnable = VK_FALSE;
        rasterizer.depthBiasConstantFactor = 0.0f;
        rasterizer.depthBiasClamp = 0.0f;
        rasterizer.depthBiasSlopeFactor = 0.0f;

        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = sampleCount;
        multisampling.minSampleShading = 1.0f;
        multisampling.pSampleMask = nullptr;
        multisampling.alphaToCoverageEnable = VK_FALSE;
        multisampling.alphaToOneEnable = VK_FALSE;

        std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments(createInfo.BlendStates.size());
        for (u32 i = 0; i < createInfo.BlendStates.size(); i++)
        {
            colorBlendAttachments[i].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            colorBlendAttachments[i].blendEnable = createInfo.BlendStates[i].BlendEnable;
            colorBlendAttachments[i].srcColorBlendFactor = Common::ConvertBlendFactor(createInfo.BlendStates[i].SrcColorBlendFactor);
            colorBlendAttachments[i].dstColorBlendFactor = Common::ConvertBlendFactor(createInfo.BlendStates[i].DstColorBlendFactor);
            colorBlendAttachments[i].colorBlendOp = Common::ConvertBlendOperation(createInfo.BlendStates[i].ColorBlendOperation);
            colorBlendAttachments[i].srcAlphaBlendFactor = Common::ConvertBlendFactor(createInfo.BlendStates[i].SrcAlphaBlendFactor);
            colorBlendAttachments[i].dstAlphaBlendFactor = Common::ConvertBlendFactor(createInfo.BlendStates[i].DstAlphaBlendFactor);
            colorBlendAttachments[i].alphaBlendOp = Common::ConvertBlendOperation(createInfo.BlendStates[i].AlphaBlendOperation);
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

        VkDescriptorSetLayout layout[1] = { m_DescriptorSetLayout.GetLayout() };
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = layout;
        pipelineLayoutInfo.pushConstantRangeCount = 0;
        pipelineLayoutInfo.pPushConstantRanges = nullptr;
        FL_VK_ENSURE_RESULT(vkCreatePipelineLayout(Context::Devices().Device(), &pipelineLayoutInfo, nullptr, &m_PipelineLayout));

        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable = createInfo.DepthTest;
        depthStencil.depthWriteEnable = createInfo.DepthWrite;
        depthStencil.depthCompareOp = Flourish::Context::ReversedZBuffer() ? VK_COMPARE_OP_GREATER_OR_EQUAL : VK_COMPARE_OP_LESS;
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
        pipelineInfo.renderPass = renderPass->GetRenderPass();
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
        pipelineInfo.basePipelineIndex = -1;

        u32 subpassCount = m_Info.CompatibleSubpasses.empty() ? renderPass->GetSubpasses().size() : m_Info.CompatibleSubpasses.size();
        m_Pipelines.resize(subpassCount);
        for (u32 i = 0; i < subpassCount; i++)
        {
            pipelineInfo.subpass = m_Info.CompatibleSubpasses.empty() ? i : m_Info.CompatibleSubpasses[i];
            
            if (i > 0)
            {
                pipelineInfo.flags = VK_PIPELINE_CREATE_DERIVATIVE_BIT;
                pipelineInfo.basePipelineHandle = m_Pipelines[0];
            }

            FL_VK_ENSURE_RESULT(vkCreateGraphicsPipelines(Context::Devices().Device(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_Pipelines[i]));
        }
    }

    GraphicsPipeline::~GraphicsPipeline()
    {
        m_DescriptorSetLayout.Shutdown();

        auto pipelines = m_Pipelines;
        auto layout = m_PipelineLayout;
        Context::DeleteQueue().Push([=]()
        {
            for (auto pipeline : pipelines)
                vkDestroyPipeline(Context::Devices().Device(), pipeline, nullptr);
            vkDestroyPipelineLayout(Context::Devices().Device(), layout, nullptr);
        }, "Graphics pipeline free");
    }
}