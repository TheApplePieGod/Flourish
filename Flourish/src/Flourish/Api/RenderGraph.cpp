#include "flpch.h"
#include "RenderGraph.h"

#include "Flourish/Backends/Vulkan/RenderGraph.h"

namespace Flourish
{
    RenderGraphNodeBuilder::RenderGraphNodeBuilder(RenderGraph* graph, CommandBuffer* buffer)
        : m_Graph(graph)
    {
        m_Node.Buffer = buffer;
    }

    RenderGraphNodeBuilder& RenderGraphNodeBuilder::AddExecutionDependency(CommandBuffer* buffer)
    {
        m_Node.ExecutionDependencies.insert(buffer->GetId());
        return *this;
    }

    RenderGraphNodeBuilder& RenderGraphNodeBuilder::AddEncoderNode(GPUWorkloadType workloadType)
    {
        m_Node.EncoderNodes.emplace_back();
        m_Node.EncoderNodes.back().WorkloadType = workloadType;
        return *this;
    }

    RenderGraphNodeBuilder& RenderGraphNodeBuilder::EncoderAddBufferRead(Buffer* buffer)
    {
        FL_ASSERT(!m_Node.EncoderNodes.empty(), "Must call AddEncoderNode first");
        m_Node.EncoderNodes.back().ReadResources.insert(buffer->GetId());
        return *this;
    }

    RenderGraphNodeBuilder& RenderGraphNodeBuilder::EncoderAddBufferWrite(Buffer* buffer)
    {
        m_Node.EncoderNodes.back().WriteResources.insert(buffer->GetId());
        return *this;
    }

    RenderGraphNodeBuilder& RenderGraphNodeBuilder::EncoderAddTextureRead(Texture* texture)
    {
        m_Node.EncoderNodes.back().ReadResources.insert(texture->GetId());
        return *this;
    }

    RenderGraphNodeBuilder& RenderGraphNodeBuilder::EncoderAddTextureWrite(Texture* texture)
    {
        m_Node.EncoderNodes.back().WriteResources.insert(texture->GetId());
        return *this;
    }

    void RenderGraphNodeBuilder::AddToGraph()
    {
        m_Graph->AddInternal(m_Node);
    }

    RenderGraphNodeBuilder RenderGraph::ConstructNewNode(CommandBuffer *buffer)
    {
        return RenderGraphNodeBuilder(this, buffer);
    }

    void RenderGraph::Clear()
    {
        m_Leaves.clear();
        m_Nodes.clear();
        m_Built = false;
    }

    void RenderGraph::AddInternal(const RenderGraphNode& addData)
    {
        if (!addData.Buffer)
        {
            FL_LOG_WARN("Adding a node to rendergraph with null commandbuffer");
            return;
        }
        u64 id = addData.Buffer->GetId();
        auto found = m_Nodes.find(id);
        if (found == m_Nodes.end())
        {
            m_Nodes.insert({ id, addData });
            m_Leaves.insert(id);
        }
        else
        {
            FL_LOG_WARN("Adding a node to rendergraph that was already added");
            return;
        }

        for (u64 parentId : addData.ExecutionDependencies)
        {
            if (parentId == 0 || m_Nodes.find(parentId) == m_Nodes.end())
                return;

            m_Leaves.erase(parentId);
        }
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
