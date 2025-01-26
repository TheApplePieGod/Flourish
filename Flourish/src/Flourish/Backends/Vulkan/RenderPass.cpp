#include "flpch.h"
#include "RenderPass.h"

#include "Flourish/Backends/Vulkan/Context.h"
#include "Flourish/Backends/Vulkan/GraphicsPipeline.h"

namespace Flourish::Vulkan
{
    RenderPass::RenderPass(const RenderPassCreateInfo& createInfo)
        : Flourish::RenderPass(createInfo)
    {
        std::vector<VkAttachmentDescription2> attachmentDescriptions = {};

        // Clamp sample count
        m_SampleCount = Common::ConvertMsaaSampleCount(createInfo.SampleCount);
        if (m_SampleCount > Context::Devices().MaxMsaaSamples())
            m_SampleCount = Context::Devices().MaxMsaaSamples();

        // MSAA textures need to be resolved
        m_UseResolve = m_SampleCount > VK_SAMPLE_COUNT_1_BIT;

        // TODO: depth resolve (https://github.com/KhronosGroup/Vulkan-Samples/blob/master/samples/performance/msaa/msaa_tutorial.md)
        // Create all depth attachment descriptions
        bool depthLoad = false;
        for (auto& attachment : createInfo.DepthAttachments)
        {
            FL_ASSERT(attachment.Format == ColorFormat::Depth, "Depth attachment must be created with the depth format");

            VkFormat colorFormat = Common::ConvertColorFormat(attachment.Format);

            if (attachment.Initialization == AttachmentInitialization::Preserve)
                depthLoad = true;

            VkAttachmentDescription2 depthAttachment{};
            depthAttachment.sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2;
            depthAttachment.format = colorFormat;
            depthAttachment.samples = m_SampleCount;
            depthAttachment.loadOp = Common::ConvertAttachmentInitialization(attachment.Initialization);
            depthAttachment.storeOp = m_UseResolve ? VK_ATTACHMENT_STORE_OP_DONT_CARE : VK_ATTACHMENT_STORE_OP_STORE;
            depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            // Spec requires initial layout to not be undefined if loadop is load
            depthAttachment.initialLayout = attachment.Initialization == AttachmentInitialization::Preserve ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED;
            depthAttachment.finalLayout = m_UseResolve ? VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            attachmentDescriptions.emplace_back(depthAttachment);

            if (m_UseResolve)
            {
                VkAttachmentDescription2 depthAttachmentResolve{};
                depthAttachmentResolve.sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2;
                depthAttachmentResolve.format = colorFormat;
                depthAttachmentResolve.samples = VK_SAMPLE_COUNT_1_BIT;
                depthAttachmentResolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                depthAttachmentResolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
                depthAttachmentResolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                depthAttachmentResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
                depthAttachmentResolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                depthAttachmentResolve.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                attachmentDescriptions.emplace_back(depthAttachmentResolve);
            }
        }

        // Create all color attachment descriptions
        bool colorLoad = false;
        for (auto& attachment : createInfo.ColorAttachments)
        {
            FL_ASSERT(
                !Texture::IsColorFormatCompressed(attachment.Format),
                "RenderPass attachment must not be a compressed image format"
            );

            VkFormat colorFormat = Common::ConvertColorFormat(attachment.Format);

            if (attachment.Initialization == AttachmentInitialization::Preserve)
                colorLoad = true;

            VkImageLayout initLayout = attachment.SupportComputeImages ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            VkAttachmentDescription2 colorAttachment{};
            colorAttachment.sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2;
            colorAttachment.format = colorFormat;
            colorAttachment.samples = m_SampleCount;
            colorAttachment.loadOp = Common::ConvertAttachmentInitialization(attachment.Initialization);
            colorAttachment.storeOp = m_UseResolve ? VK_ATTACHMENT_STORE_OP_DONT_CARE : VK_ATTACHMENT_STORE_OP_STORE;
            colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            colorAttachment.initialLayout = attachment.Initialization == AttachmentInitialization::Preserve ? initLayout : VK_IMAGE_LAYOUT_UNDEFINED;
            colorAttachment.finalLayout = m_UseResolve ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : initLayout;
            attachmentDescriptions.emplace_back(colorAttachment);

            if (m_UseResolve)
            {
                VkAttachmentDescription2 colorAttachmentResolve{};
                colorAttachmentResolve.sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2;
                colorAttachmentResolve.format = colorFormat;
                colorAttachmentResolve.samples = VK_SAMPLE_COUNT_1_BIT;
                colorAttachmentResolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                colorAttachmentResolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
                colorAttachmentResolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                colorAttachmentResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
                colorAttachmentResolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                colorAttachmentResolve.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                attachmentDescriptions.emplace_back(colorAttachmentResolve);
            }
        }

        // Build the subpass dependency list and connect all of their attachments
        // TODO: we need to validate that the attachment indices line up with what were actually submitted
        // because there can be issues with referencing a depth attachment but the rp was created without one
        u32 subpassCount = createInfo.Subpasses.size();
        u32 colorAttachmentStartIndex = createInfo.DepthAttachments.size() * (m_UseResolve ? 2 : 1);
        std::vector<VkSubpassDescription2> subpasses(subpassCount);
        std::vector<VkSubpassDependency2> dependencies(subpassCount);
        std::vector<VkAttachmentReference2> inputAttachmentRefs;
        std::vector<VkAttachmentReference2> outputAttachmentRefs;
        std::vector<VkAttachmentReference2> outputResolveAttachmentRefs;
        std::vector<VkAttachmentReference2> depthAttachmentRefs;
        std::vector<VkAttachmentReference2> depthAttachmentResolveRefs;
        std::vector<VkSubpassDescriptionDepthStencilResolve> depthAttachmentResolveDescs;
        inputAttachmentRefs.reserve(subpassCount * createInfo.ColorAttachments.size());
        outputAttachmentRefs.reserve(subpassCount * createInfo.ColorAttachments.size());
        outputResolveAttachmentRefs.reserve(subpassCount * createInfo.ColorAttachments.size());
        depthAttachmentRefs.reserve(createInfo.Subpasses.size());
        depthAttachmentResolveRefs.reserve(createInfo.Subpasses.size());
        depthAttachmentResolveDescs.reserve(createInfo.Subpasses.size());
        for (u32 i = 0; i < subpasses.size(); i++)
        {
            FL_ASSERT(i != 0 || inputAttachmentRefs.size() == 0, "Subpass 0 cannot have input attachments");

            depthAttachmentRefs.push_back({
                VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,
                nullptr,
                VK_ATTACHMENT_UNUSED,
                VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL
            });
            depthAttachmentResolveRefs.push_back(depthAttachmentRefs.back());
            depthAttachmentResolveDescs.emplace_back();

            u32 inputAttachmentCount = 0;
            u32 outputAttachmentCount = 0;
            u32 inputColorCount = 0;
            u32 inputDepthCount = 0;
            u32 outputColorCount = 0;
            u32 outputDepthCount = 0;
            for (u32 j = 0; j < createInfo.Subpasses[i].InputAttachments.size(); j++)
            {
                auto& attachment = createInfo.Subpasses[i].InputAttachments[j];
                if (attachment.Type == SubpassAttachmentType::Color)
                {
                    inputAttachmentRefs.push_back({
                        VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,
                        nullptr,
                        m_UseResolve
                            ? colorAttachmentStartIndex + (attachment.AttachmentIndex * 2) + 1
                            : colorAttachmentStartIndex + attachment.AttachmentIndex,
                        m_Info.ColorAttachments[attachment.AttachmentIndex].SupportComputeImages
                            ? VK_IMAGE_LAYOUT_GENERAL
                            : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        VK_IMAGE_ASPECT_COLOR_BIT
                    });
                    inputColorCount++;
                }
                else
                {
                    inputAttachmentRefs.push_back({
                        VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,
                        nullptr,
                        m_UseResolve
                            ? (attachment.AttachmentIndex * 2) + 1
                            : attachment.AttachmentIndex,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        VK_IMAGE_ASPECT_DEPTH_BIT                        
                    });
                    inputDepthCount++;
                }
                inputAttachmentCount++;
            }
            for (u32 j = 0; j < createInfo.Subpasses[i].OutputAttachments.size(); j++)
            {
                auto& attachment = createInfo.Subpasses[i].OutputAttachments[j];
                if (attachment.Type == SubpassAttachmentType::Depth)
                {
                    FL_ASSERT(depthAttachmentRefs[i].attachment == VK_ATTACHMENT_UNUSED, "Cannot bind more than one depth attachment to the output of a subpass");
                    depthAttachmentRefs[i].attachment = m_UseResolve ? (attachment.AttachmentIndex * 2) : attachment.AttachmentIndex;
                    if (m_UseResolve)
                    {
                        depthAttachmentResolveRefs[i].attachment = (attachment.AttachmentIndex * 2) + 1;

                        depthAttachmentResolveDescs[i].sType = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_DEPTH_STENCIL_RESOLVE;
                        depthAttachmentResolveDescs[i].pNext = nullptr;
                        depthAttachmentResolveDescs[i].depthResolveMode = VK_RESOLVE_MODE_MAX_BIT;
                        depthAttachmentResolveDescs[i].stencilResolveMode = VK_RESOLVE_MODE_MAX_BIT;
                        depthAttachmentResolveDescs[i].pDepthStencilResolveAttachment = &depthAttachmentResolveRefs[i];
                    }
                    outputDepthCount++;
                }
                else
                {
                    outputAttachmentRefs.push_back({
                        // Always outputting to the normal image attachment
                        VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,
                        nullptr,
                        m_UseResolve
                            ? colorAttachmentStartIndex + (attachment.AttachmentIndex * 2)
                            : colorAttachmentStartIndex + attachment.AttachmentIndex,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
                    });
                    outputResolveAttachmentRefs.push_back({
                        VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,
                        nullptr,
                        m_UseResolve
                            ? colorAttachmentStartIndex + (attachment.AttachmentIndex * 2) + 1
                            : VK_ATTACHMENT_UNUSED,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
                    });
                    outputAttachmentCount++;
                    outputColorCount++;
                }
            }

            subpasses[i] = {};
            subpasses[i].sType = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2;
            subpasses[i].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
            subpasses[i].colorAttachmentCount = outputAttachmentCount;
            subpasses[i].pColorAttachments = outputAttachmentRefs.data() + outputAttachmentRefs.size() - outputAttachmentCount;
            subpasses[i].pResolveAttachments = outputResolveAttachmentRefs.data() + outputResolveAttachmentRefs.size() - outputAttachmentCount;
            subpasses[i].pDepthStencilAttachment = depthAttachmentRefs.data() + i;
            subpasses[i].inputAttachmentCount = inputAttachmentCount;
            subpasses[i].pInputAttachments = inputAttachmentRefs.data() + inputAttachmentRefs.size() - inputAttachmentCount;

            if (depthAttachmentResolveRefs[i].attachment != VK_ATTACHMENT_UNUSED)
                subpasses[i].pNext = &depthAttachmentResolveDescs[i];

            dependencies[i] = {};
            dependencies[i].sType = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2;
            dependencies[i].srcSubpass = static_cast<u32>(i - 1);
            dependencies[i].dstSubpass = static_cast<u32>(i);

            // This needs to be not zero otherwise we get a validation error
            dependencies[i].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

            // Could be more granular here by checking load attachments by subpass
            dependencies[i].srcAccessMask = 0;
            if (inputColorCount > 0)
            {
                dependencies[i].srcStageMask |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                dependencies[i].srcAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            }
            if (inputDepthCount > 0)
            {
                dependencies[i].srcStageMask |= VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
                dependencies[i].srcAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            }
            
            dependencies[i].dstAccessMask = 0;
            dependencies[i].dstStageMask = 0;

            if (outputColorCount > 0)
            {
                dependencies[i].dstStageMask |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                dependencies[i].dstAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            }
            if (outputDepthCount > 0)
            {
                dependencies[i].dstStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
                dependencies[i].dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            }

            if (colorLoad)
                dependencies[i].dstAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
            if (depthLoad)
                dependencies[i].dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;

            dependencies[i].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

            if (i == 0)
                dependencies[i].srcSubpass = VK_SUBPASS_EXTERNAL;

            // Support for subpass reads. Could again be more granular by checking if this
            // ever gets used as a read, etc.
            if (i != subpasses.size() - 1 && subpasses.size() > 1)
            {
                dependencies[i].dstStageMask |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
                dependencies[i].dstAccessMask |= VK_ACCESS_SHADER_READ_BIT;
            }
        }

        VkRenderPassCreateInfo2 renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2;
        renderPassInfo.attachmentCount = static_cast<u32>(attachmentDescriptions.size());
        renderPassInfo.pAttachments = attachmentDescriptions.data();
        renderPassInfo.subpassCount = static_cast<u32>(subpasses.size());
        renderPassInfo.pSubpasses = subpasses.data();
        renderPassInfo.dependencyCount = static_cast<u32>(dependencies.size());;
        renderPassInfo.pDependencies = dependencies.data();
        
        if (!FL_VK_CHECK_RESULT(vkCreateRenderPass2KHR(
            Context::Devices().Device(),
            &renderPassInfo,
            nullptr,
            &m_RenderPass
        ), "RenderPass create renderpass"))
            throw std::exception();
    }

    RenderPass::~RenderPass()
    {
        auto renderPass = m_RenderPass;
        Context::FinalizerQueue().Push([=]()
        {
            if (renderPass)
                vkDestroyRenderPass(Context::Devices().Device(), renderPass, nullptr);
        }, "RenderPass free");
    }

    std::unique_ptr<Flourish::GraphicsPipeline> RenderPass::CreatePipeline(const GraphicsPipelineCreateInfo& createInfo)
    {
        #ifdef FL_ENABLE_ASSERTS
        for (u32 subpass : createInfo.CompatibleSubpasses)
        {
            u32 count = 0;
            for (auto& attachment : m_Info.Subpasses[subpass].OutputAttachments)
                if (attachment.Type == SubpassAttachmentType::Color)
                    count++;
            FL_ASSERT(
                createInfo.BlendStates.size() == count,
                "Graphics pipeline blend state count must match the number of color attachments defined for this render pass"
            );
        }

        if (!Flourish::Context::FeatureTable().IndependentBlend)
        {
            for (u32 i = 1; i < createInfo.BlendStates.size(); i++)
            {
                FL_ASSERT(
                    createInfo.BlendStates[i - 1] == createInfo.BlendStates[i],
                    "If the IndependentBlend feature is not enabled, blend states for each attachment must all be identical"
                )
            }
        }
        #endif

        return std::make_unique<GraphicsPipeline>(createInfo, this);
    }
}
