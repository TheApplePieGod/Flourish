#include "flpch.h"
#include "RenderGraph.h"

#include "Flourish/Backends/Vulkan/RenderGraph.h"
#include "Flourish/Backends/Vulkan/Framebuffer.h"

namespace Flourish
{
    RenderGraphNodeBuilder::RenderGraphNodeBuilder()
    {}

    RenderGraphNodeBuilder::RenderGraphNodeBuilder(RenderGraph* graph, CommandBuffer* buffer)
        : m_Graph(graph)
    {
        m_Node.Buffer = buffer;
    }

    RenderGraphNodeBuilder& RenderGraphNodeBuilder::Reset()
    {
        m_Node = RenderGraphNode();
        return *this;
    }

    RenderGraphNodeBuilder& RenderGraphNodeBuilder::SetCommandBuffer(CommandBuffer* buffer)
    {
        m_Node.Buffer = buffer;
        return *this;
    }

    RenderGraphNodeBuilder& RenderGraphNodeBuilder::AddExecutionDependency(const CommandBuffer* buffer)
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

    RenderGraphNodeBuilder& RenderGraphNodeBuilder::EncoderAddBufferRead(const Buffer* buffer)
    {
        FL_ASSERT(!m_Node.EncoderNodes.empty(), "Must call AddEncoderNode first");
        m_Node.EncoderNodes.back().ReadResources.insert(buffer->GetId());
        return *this;
    }

    RenderGraphNodeBuilder& RenderGraphNodeBuilder::EncoderAddBufferWrite(const Buffer* buffer)
    {
        m_Node.EncoderNodes.back().WriteResources.insert(buffer->GetId());
        return *this;
    }

    RenderGraphNodeBuilder& RenderGraphNodeBuilder::EncoderAddTextureRead(const Texture* texture)
    {
        m_Node.EncoderNodes.back().ReadResources.insert(texture->GetId());
        return *this;
    }

    RenderGraphNodeBuilder& RenderGraphNodeBuilder::EncoderAddTextureWrite(const Texture* texture)
    {
        m_Node.EncoderNodes.back().WriteResources.insert(texture->GetId());
        return *this;
    }

    RenderGraphNodeBuilder& RenderGraphNodeBuilder::EncoderAddFramebuffer(const Framebuffer* framebuffer)
    {
        RenderPass* pass = framebuffer->GetRenderPass();

        for (u32 i = 0; i < framebuffer->GetColorAttachments().size(); i++)
        {
            auto& tex = framebuffer->GetColorAttachments()[i].Texture;
            if (!tex)
                continue;
            if (pass->GetColorAttachment(i).Initialization == Flourish::AttachmentInitialization::Preserve)
                EncoderAddTextureRead(tex.get());
            EncoderAddTextureWrite(tex.get());
        }

        for (u32 i = 0; i < framebuffer->GetDepthAttachments().size(); i++)
        {
            auto& tex = framebuffer->GetDepthAttachments()[i].Texture;
            if (!tex)
                continue;
            if (pass->GetDepthAttachment(i).Initialization == Flourish::AttachmentInitialization::Preserve)
                EncoderAddTextureRead(tex.get());
            EncoderAddTextureWrite(tex.get());
        }

        return *this;
    }

    void RenderGraphNodeBuilder::AddToGraph() const
    {
        FL_ASSERT(m_Graph, "m_Graph is null, call AddToGraph(graph)");
        m_Graph->AddInternal(m_Node);
    }

    void RenderGraphNodeBuilder::AddToGraph(RenderGraph* graph) const
    {
        graph->AddInternal(m_Node);
    }

    RenderGraphNodeBuilder RenderGraph::ConstructNewNode(CommandBuffer *buffer)
    {
        return RenderGraphNodeBuilder(this, buffer);
    }

    void RenderGraph::AddExecutionDependency(const CommandBuffer* buffer, const CommandBuffer* dependsOn)
    {
        if (!buffer)
        {
            FL_LOG_WARN("AddExecutionDependency buffer is null");
            return;
        }
        if (!dependsOn)
        {
            FL_LOG_WARN("AddExecutionDependency dependsOn is null");
            return;
        }

        auto found = m_Nodes.find(buffer->GetId());
        if (found == m_Nodes.end())
        {
            FL_LOG_WARN("AddExecutionDependency buffer is not in graph");
            return;
        }

        if (m_Nodes.find(dependsOn->GetId()) == m_Nodes.end())
        {
            FL_LOG_WARN("AddExecutionDependency dependsOn is not in graph");
            return;
        }

        m_Leaves.erase(dependsOn->GetId());
        found->second.ExecutionDependencies.insert(dependsOn->GetId());
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
