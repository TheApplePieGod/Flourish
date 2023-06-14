#include "flpch.h"
#include "RenderGraph.h"

#include "Flourish/Backends/Vulkan/RenderGraph.h"

namespace Flourish
{
    void RenderGraph::Clear()
    {
        m_Leaves.clear();
        m_Nodes.clear();
    }

    void RenderGraph::AddCommandBuffer(CommandBuffer* buffer, CommandBuffer* parent)
    {
        u64 bufId = reinterpret_cast<u64>(buffer);
        if (bufId == 0)
            return;
        auto found = m_Nodes.find(bufId);
        if (found == m_Nodes.end())
        {
            m_Nodes.insert({ bufId, { buffer, nullptr } });
            if (!parent)
                m_Leaves.insert(bufId);
        }

        u64 parentId = reinterpret_cast<u64>(parent);
        if (parentId == 0)
            return;

        m_Leaves.erase(parentId);
        found->second.Dependencies.insert(parentId);
    }

    void RenderGraph::AddRenderContext(RenderContext* context, CommandBuffer* parent)
    {
        u64 contextId = reinterpret_cast<u64>(context);
        if (contextId == 0)
            return;
        auto found = m_Nodes.find(contextId);
        if (found == m_Nodes.end())
        {
            m_Nodes.insert({ contextId, { nullptr, context } });
            if (!parent)
                m_Leaves.insert(contextId);
        }

        u64 parentId = reinterpret_cast<u64>(parent);
        if (parentId == 0)
            return;

        m_Leaves.erase(parentId);
        found->second.Dependencies.insert(parentId);
    }

    std::shared_ptr<RenderGraph> RenderGraph::Create(const RenderGraphCreateInfo& createInfo)
    {
        FL_ASSERT(Context::BackendType() != BackendType::None, "Must initialize Context before creating a RenderGraph");

        try
        {
            switch (Context::BackendType())
            {
                case BackendType::Vulkan: { return std::make_shared<Vulkan::RenderGraph>(createInfo); }
            }
        }
        catch (const std::exception& e) {}

        FL_ASSERT(false, "Failed to create RenderGraph");
        return nullptr;
    }
}
