#include "flpch.h"
#include "RenderPass.h"

#include "Flourish/Backends/Vulkan/Util/Context.h"
#include "Flourish/Backends/Vulkan/GraphicsPipeline.h"

namespace Flourish::Vulkan
{
    RenderPass::RenderPass(const RenderPassCreateInfo& createInfo, bool rendersToSwapchain)
        : Flourish::RenderPass(createInfo)
    {
        u32 attachmentIndex = 0;
        std::vector<VkAttachmentReference> colorAttachmentRefs = {};
        std::vector<VkAttachmentReference> resolveAttachmentRefs = {};
        std::vector<VkAttachmentDescription> attachmentDescriptions = {};

        // Clamp sample count
        m_SampleCount = Common::ConvertMsaaSampleCount(createInfo.SampleCount);
        if (m_SampleCount > Context::Devices().MaxMsaaSamples())
            m_SampleCount = Context::Devices().MaxMsaaSamples();

        // MSAA textures need to be resolved
        m_UseResolve = m_SampleCount > VK_SAMPLE_COUNT_1_BIT;

        // TODO: depth resolve (https://github.com/KhronosGroup/Vulkan-Samples/blob/master/samples/performance/msaa/msaa_tutorial.md)
        // Create all depth attachment descriptions
        for (auto& attachment : createInfo.DepthAttachments)
        {
            VkAttachmentDescription depthAttachment{};
            depthAttachment.format = DepthFormat;
            depthAttachment.samples = m_SampleCount;
            depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL;
            attachmentDescriptions.emplace_back(depthAttachment);
        }

        // Create all color attachment descriptions
        for (auto& attachment : createInfo.ColorAttachments)
        {
            VkFormat colorFormat = Common::ConvertColorFormat(attachment.Format);

            VkAttachmentDescription colorAttachment{};
            colorAttachment.format = colorFormat;
            colorAttachment.samples = m_SampleCount;
            colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            colorAttachment.storeOp = (m_UseResolve ? VK_ATTACHMENT_STORE_OP_DONT_CARE : VK_ATTACHMENT_STORE_OP_STORE);
            colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            colorAttachment.finalLayout = rendersToSwapchain ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; //VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            attachmentDescriptions.emplace_back(colorAttachment);

            if (m_UseResolve)
            {
                VkAttachmentDescription colorAttachmentResolve{};
                colorAttachmentResolve.format = colorFormat;
                colorAttachmentResolve.samples = VK_SAMPLE_COUNT_1_BIT;
                colorAttachmentResolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                colorAttachmentResolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
                colorAttachmentResolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                colorAttachmentResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
                colorAttachmentResolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                colorAttachmentResolve.finalLayout = rendersToSwapchain ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; //VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                attachmentDescriptions.emplace_back(colorAttachmentResolve);
            }

            VkAttachmentReference colorAttachmentRef{};
            colorAttachmentRef.attachment = attachmentIndex++;
            colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            colorAttachmentRefs.emplace_back(colorAttachmentRef);

            VkAttachmentReference colorAttachmentResolveRef{};
            colorAttachmentResolveRef.attachment = m_UseResolve ? attachmentIndex++ : VK_ATTACHMENT_UNUSED;
            colorAttachmentResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            resolveAttachmentRefs.emplace_back(colorAttachmentResolveRef);
        }

        // Build the subpass dependency graph and connect all of their attachments
        u32 subpassCount = createInfo.Subpasses.size();
        u32 colorAttachmentStartIndex = createInfo.DepthAttachments.size();
        std::vector<VkSubpassDescription> subpasses(subpassCount);
        std::vector<VkSubpassDependency> dependencies(subpassCount);
        std::vector<VkAttachmentReference> inputAttachmentRefs;
        std::vector<VkAttachmentReference> outputAttachmentRefs;
        std::vector<VkAttachmentReference> outputResolveAttachmentRefs;
        std::vector<VkAttachmentReference> depthAttachmentRefs;
        inputAttachmentRefs.reserve(subpassCount * createInfo.ColorAttachments.size());
        outputAttachmentRefs.reserve(subpassCount * createInfo.ColorAttachments.size());
        outputResolveAttachmentRefs.reserve(subpassCount * createInfo.ColorAttachments.size());
        depthAttachmentRefs.reserve(createInfo.Subpasses.size());
        for (u32 i = 0; i < subpasses.size(); i++)
        {
            FL_ASSERT(i != 0 || inputAttachmentRefs.size() == 0, "Subpass 0 cannot have input attachments");

            depthAttachmentRefs.push_back({ VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL });

            u32 inputAttachmentCount = 0;
            u32 outputAttachmentCount = 0;
            for (u32 j = 0; j < createInfo.Subpasses[i].InputAttachments.size(); j++)
            {
                auto& attachment = createInfo.Subpasses[i].InputAttachments[j];
                if (attachment.Type == SubpassAttachmentType::Color)
                {
                    inputAttachmentRefs.push_back({
                        m_UseResolve
                            // Every color attachment has a resolve regardless of whether or not
                            // it is used so we can use a simple formula to figure out the attachment index
                            ? colorAttachmentStartIndex + (attachment.AttachmentIndex * 2) + 1
                            : colorAttachmentStartIndex + (attachment.AttachmentIndex * 2),
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                    });
                }
                else
                {
                    inputAttachmentRefs.push_back({
                        // Depth attachments don't have resolves yet
                        attachment.AttachmentIndex,
                        VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL
                    });
                }
                inputAttachmentCount++;
            }
            for (u32 j = 0; j < createInfo.Subpasses[i].OutputAttachments.size(); j++)
            {
                auto& attachment = createInfo.Subpasses[i].OutputAttachments[j];
                if (attachment.Type == SubpassAttachmentType::Depth)
                {
                    FL_ASSERT(depthAttachmentRefs[i].attachment == VK_ATTACHMENT_UNUSED, "Cannot bind more than one depth attachment to the output of a subpass");
                    depthAttachmentRefs[i].attachment = attachment.AttachmentIndex;
                }
                else
                {
                    outputAttachmentRefs.push_back({
                        // Always outputting to the normal image attachment
                        colorAttachmentStartIndex + (attachment.AttachmentIndex * 2),
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
                    });
                    outputResolveAttachmentRefs.push_back({
                        m_UseResolve
                            ? colorAttachmentStartIndex + (attachment.AttachmentIndex * 2) + 1
                            : VK_ATTACHMENT_UNUSED,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
                    });
                    outputAttachmentCount++;
                }
            }

            subpasses[i] = {};
            subpasses[i].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
            subpasses[i].colorAttachmentCount = outputAttachmentCount;
            subpasses[i].pColorAttachments = outputAttachmentRefs.data() + outputAttachmentRefs.size() - outputAttachmentCount;
            subpasses[i].pResolveAttachments = outputResolveAttachmentRefs.data() + outputResolveAttachmentRefs.size() - outputAttachmentCount;
            subpasses[i].pDepthStencilAttachment = depthAttachmentRefs.data() + i;
            subpasses[i].inputAttachmentCount = inputAttachmentCount;
            subpasses[i].pInputAttachments = inputAttachmentRefs.data() + inputAttachmentRefs.size() - inputAttachmentCount;

            dependencies[i] = {};
            dependencies[i].srcSubpass = static_cast<u32>(i - 1);
            dependencies[i].dstSubpass = static_cast<u32>(i);
            dependencies[i].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dependencies[i].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            dependencies[i].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            dependencies[i].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            dependencies[i].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

            if (i == 0)
            {
                dependencies[i].srcSubpass = VK_SUBPASS_EXTERNAL;
                dependencies[i].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
                dependencies[i].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
                dependencies[i].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                dependencies[i].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT; //| VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            }
            if (i == subpasses.size() - 1)
            {
                dependencies[i].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT; //| VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                dependencies[i].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
                dependencies[i].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
            }
        }

        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = static_cast<u32>(attachmentDescriptions.size());
        renderPassInfo.pAttachments = attachmentDescriptions.data();
        renderPassInfo.subpassCount = static_cast<u32>(subpasses.size());
        renderPassInfo.pSubpasses = subpasses.data();
        renderPassInfo.dependencyCount = static_cast<u32>(dependencies.size());;
        renderPassInfo.pDependencies = dependencies.data();
          
        FL_VK_ENSURE_RESULT(vkCreateRenderPass(Context::Devices().Device(), &renderPassInfo, nullptr, &m_RenderPass));
    }

    RenderPass::~RenderPass()
    {
        auto renderPass = m_RenderPass;
        Context::DeleteQueue().Push([=]()
        {
            vkDestroyRenderPass(Context::Devices().Device(), renderPass, nullptr);
        });
    }

    std::shared_ptr<Flourish::GraphicsPipeline> RenderPass::CreatePipeline(const GraphicsPipelineCreateInfo& createInfo)
    {
        FL_ASSERT(
            createInfo.BlendStates.size() == m_Info.ColorAttachments.size(),
            "Graphics pipeline blend state count must match the number of color attachments defined for this render pass"
        );

        return std::make_shared<GraphicsPipeline>(
            createInfo,
            m_RenderPass,
            m_SampleCount,
            static_cast<u32>(m_Info.Subpasses.size())
        );
    }
}