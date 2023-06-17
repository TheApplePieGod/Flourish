#pragma once

#include "Flourish/Api/CommandBuffer.h"

namespace Flourish
{
    enum class RenderGraphUsageType
    {
        Once = 0, // Build once use once
        PerFrame, // Build once use every frame
        BuildPerFrame // Build and use every frame
    };

    struct RenderGraphCreateInfo
    {
        RenderGraphUsageType Usage = RenderGraphUsageType::PerFrame;
    };

    struct RenderGraphEncoderNode
    {
        GPUWorkloadType WorkloadType;
        std::unordered_set<u64> ReadResources;
        std::unordered_set<u64> WriteResources;
    };

    struct RenderGraphNode
    {
        CommandBuffer* Buffer = nullptr;
        std::unordered_set<u64> ExecutionDependencies;
        std::vector<RenderGraphEncoderNode> EncoderNodes;
    };

    class Buffer;
    class Texture;
    class RenderGraph;
    class RenderGraphNodeBuilder
    {
    public:
        RenderGraphNodeBuilder(RenderGraph* graph, CommandBuffer* buffer);

        RenderGraphNodeBuilder& AddExecutionDependency(CommandBuffer* buffer);

        RenderGraphNodeBuilder& AddEncoderNode(GPUWorkloadType workloadType);
        RenderGraphNodeBuilder& EncoderAddBufferRead(Buffer* buffer);
        RenderGraphNodeBuilder& EncoderAddBufferWrite(Buffer* buffer);
        RenderGraphNodeBuilder& EncoderAddTextureRead(Texture* texture);
        RenderGraphNodeBuilder& EncoderAddTextureWrite(Texture* texture);

        void AddToGraph();

    private:
        RenderGraphNode m_Node;
        RenderGraph* m_Graph;
    };

    class RenderGraph
    {
    public:
        RenderGraph(const RenderGraphCreateInfo& createInfo)
            : m_Info(createInfo)
        {}
        virtual ~RenderGraph() = default;

        RenderGraphNodeBuilder ConstructNewNode(CommandBuffer* buffer);
        void Clear();

        virtual void Build() = 0;

        // TS
        inline bool IsBuild() const { return m_Built; }
        inline const RenderGraphNode& GetNode(u64 id) const { return m_Nodes.at(id); }
        inline const auto& GetNodes() const { return m_Nodes; }

    public:
        static std::shared_ptr<RenderGraph> Create(const RenderGraphCreateInfo& createInfo);

    private:
        void AddInternal(const RenderGraphNode& addData);

    protected:
        RenderGraphCreateInfo m_Info;
        std::unordered_set<u64> m_Leaves;
        std::unordered_map<u64, RenderGraphNode> m_Nodes;
        bool m_Built = false;

        friend class RenderGraphNodeBuilder;
    };
}
