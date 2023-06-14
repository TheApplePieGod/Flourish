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
        AddInternal(
            reinterpret_cast<u64>(buffer),
            reinterpret_cast<u64>(parent),
            { buffer }
        );
    }

    void RenderGraph::AddInternal(u64 id, u64 parentId, const Node& addData)
    {
        if (id == 0)
            return;
        auto found = m_Nodes.find(id);
        if (found == m_Nodes.end())
        {
            m_Nodes.insert({ id, addData });
            m_Leaves.insert(id);
            found = m_Nodes.find(id);
        }

        if (parentId == 0 || m_Nodes.find(parentId) == m_Nodes.end())
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
