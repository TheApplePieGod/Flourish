#include "flpch.h"
#include "RenderPass.h"

#include "Flourish/Api/Context.h"
#include "Flourish/Backends/Vulkan/RenderPass.h"

namespace Flourish
{
    GraphicsPipeline* RenderPass::CreatePipeline(std::string_view name, const GraphicsPipelineCreateInfo& createInfo)
    {
        std::lock_guard lock(m_PipelineLock);
        std::string nameStr(name.data(), name.size());
        FL_ASSERT(
            m_Pipelines.find(nameStr) == m_Pipelines.end(),
            "Cannot create pipeline to RenderPass, name already exists: %s",
            name.data()
        );

        FL_LOG_DEBUG("Registering graphics pipeline '%s' to RenderPass", name.data());

        std::unique_ptr<GraphicsPipeline> newPipeline;
        try
        {
            newPipeline = CreatePipeline(createInfo);
        }
        catch (const std::exception& e)
        {
            FL_ASSERT(false, "Failed to create graphics pipeline");
            return nullptr;
        }

        m_Pipelines[nameStr] = std::move(newPipeline);
        return m_Pipelines[nameStr].get();
    }

    GraphicsPipeline* RenderPass::GetPipeline(std::string_view name)
    {
        std::shared_lock lock(m_PipelineLock);
        std::string nameStr(name.data(), name.size());
        FL_ASSERT(
            m_Pipelines.find(nameStr) != m_Pipelines.end(),
            "Cannot get pipeline from RenderPass, not created: %s",
            name.data()
        );

        return m_Pipelines[nameStr].get();
    }

    u32 RenderPass::GetColorAttachmentCount(u32 subpassIndex)
    {
        u32 count = 0;
        for (auto& attachment : m_Info.Subpasses[subpassIndex].OutputAttachments)
            if (attachment.Type == SubpassAttachmentType::Color)
                count++;
        return count;
    }

    std::shared_ptr<RenderPass> RenderPass::Create(const RenderPassCreateInfo& createInfo)
    {
        FL_ASSERT(Context::BackendType() != BackendType::None, "Must initialize Context before creating a RenderPass");

        switch (Context::BackendType())
        {
            default: return nullptr;
            case BackendType::Vulkan: { return std::make_shared<Vulkan::RenderPass>(createInfo); }
        }

        FL_ASSERT(false, "RenderPass not supported for backend");
        return nullptr;
    }
}
