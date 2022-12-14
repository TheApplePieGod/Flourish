#pragma once

#include "Flourish/Api/Pipeline.h"

namespace Flourish
{
    enum class VertexTopology
    {
        None = 0,
        TriangleList, TriangleStrip, TriangleFan, PointList, LineList, LineStrip
    };

    enum class CullMode
    {
        None = 0,
        Backface, Frontface, BackAndFront
    };

    enum class WindingOrder
    {
        None = 0,
        Clockwise, CounterClockwise
    };

    enum class BlendFactor
    {
        Zero = 0,
        One,
        SrcColor, OneMinusSrcColor, DstColor, OneMinusDstColor,
        SrcAlpha, OneMinusSrcAlpha, DstAlpha, OneMinusDstAlpha
    };

    enum class BlendOperation
    {
        Add = 0,
        Subtract, ReverseSubtract, Min, Max
    };

    struct AttachmentBlendState
    {
        bool BlendEnable;
        BlendFactor SrcColorBlendFactor = BlendFactor::SrcAlpha;
        BlendFactor DstColorBlendFactor = BlendFactor::OneMinusSrcAlpha;
        BlendFactor SrcAlphaBlendFactor = BlendFactor::One;
        BlendFactor DstAlphaBlendFactor = BlendFactor::Zero;
        BlendOperation ColorBlendOperation = BlendOperation::Add;
        BlendOperation AlphaBlendOperation = BlendOperation::Add;
        
        bool operator==(const AttachmentBlendState& other) const;
    };

    struct GraphicsPipelineCreateInfo
    {
        std::shared_ptr<Shader> VertexShader;
        std::shared_ptr<Shader> FragmentShader;

        bool VertexInput;
        VertexTopology VertexTopology;
        BufferLayout VertexLayout;

        // One state is required per output attachment
        std::vector<AttachmentBlendState> BlendStates; 

        bool DepthTest;
        bool DepthWrite;
        CullMode CullMode;
        WindingOrder WindingOrder;
       
        // If empty, will try and create for all subpasses
        std::vector<u32> CompatibleSubpasses;
    };

    class Texture;
    class GraphicsPipeline : public Pipeline
    {
    public:
        GraphicsPipeline(const GraphicsPipelineCreateInfo& createInfo)
            : m_Info(createInfo)
        {
            FL_ASSERT(createInfo.VertexShader && createInfo.FragmentShader, "Must specify both vertex and fragment shaders");
            ConsolidateReflectionData();
        }
        virtual ~GraphicsPipeline() = default;

        inline VertexTopology GetVertexTopology() const { return m_Info.VertexTopology; }
        inline CullMode GetCullMode() const { return m_Info.CullMode; }
        inline WindingOrder GetWindingOrder() const { return m_Info.WindingOrder; }
        inline bool IsDepthTestEnabled() const { return m_Info.DepthTest; }
        inline bool IsDepthWriteEnabled() const { return m_Info.DepthWrite; }
        inline u32 GetVertexLayoutStride() const { return m_Info.VertexLayout.GetStride(); }
        inline const auto& GetBlendStates() const { return m_Info.BlendStates; }

    protected:
        GraphicsPipelineCreateInfo m_Info;

    private:
        void ConsolidateReflectionData();
    };
}