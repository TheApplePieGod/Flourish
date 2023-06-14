#pragma once

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

    class CommandBuffer;
    class RenderContext;
    class RenderGraph
    {
    public:
        struct Node
        {
            CommandBuffer* Buffer;
            RenderContext* Context;
            std::unordered_set<u64> Dependencies;
        };

    public:
        RenderGraph(const RenderGraphCreateInfo& createInfo)
            : m_Info(createInfo)
        {}
        virtual ~RenderGraph() = default;

        void Clear();
        void AddCommandBuffer(CommandBuffer* buffer, CommandBuffer* parent = nullptr);
        void AddRenderContext(RenderContext* context, CommandBuffer* parent = nullptr);

        virtual void Build() = 0;

        // TS
        inline bool IsBuild() const { return m_Built; }
        inline const Node& GetNode(u64 id) const { return m_Nodes.at(id); }

    public:
        static std::shared_ptr<RenderGraph> Create(const RenderGraphCreateInfo& createInfo);

    protected:
        RenderGraphCreateInfo m_Info;
        std::unordered_set<u64> m_Leaves;
        std::unordered_map<u64, Node> m_Nodes;
        bool m_Built = false;
    };
}
