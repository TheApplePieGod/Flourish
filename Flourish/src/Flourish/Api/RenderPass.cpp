#include "flpch.h"
#include "RenderPass.h"

#include "Flourish/Api/Context.h"
#include "Flourish/Backends/Vulkan/RenderPass.h"

namespace Flourish
{
    std::shared_ptr<GraphicsPipeline> RenderPass::RegisterPipeline(std::string_view name, const GraphicsPipelineCreateInfo& createInfo)
    {
        std::lock_guard lock(m_PipelineLock);
        std::string nameStr(name.data(), name.size());
        FL_ASSERT(
            m_Pipelines.find(nameStr) == m_Pipelines.end(),
            "Cannot register pipeline to RenderPass, name already exists: %s",
            name.data()
        );

        FL_LOG_DEBUG("Registering graphics pipeline '%s' to RenderPass", name.data());

        auto newPipeline = CreatePipeline(createInfo);
        m_Pipelines[nameStr] = newPipeline;
        return newPipeline;
    }

    std::shared_ptr<GraphicsPipeline> RenderPass::GetPipeline(std::string_view name)
    {
        std::shared_lock lock(m_PipelineLock);
        std::string nameStr(name.data(), name.size());
        FL_ASSERT(
            m_Pipelines.find(nameStr) != m_Pipelines.end(),
            "Cannot get pipeline from RenderPass, not registered: %s",
            name.data()
        );

        return m_Pipelines[nameStr];
    }

    std::shared_ptr<RenderPass> RenderPass::Create(const RenderPassCreateInfo& createInfo)
    {
        FL_ASSERT(Context::BackendType() != BackendType::None, "Must initialize Context before creating a RenderPass");

        switch (Context::BackendType())
        {
            case BackendType::Vulkan: { return std::make_shared<Vulkan::RenderPass>(createInfo); }
        }

        FL_ASSERT(false, "RenderPass not supported for backend");
        return nullptr;
    }
}