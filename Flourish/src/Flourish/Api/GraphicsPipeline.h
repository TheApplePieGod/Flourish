#pragma once

#include "Flourish/Api/PipelineCommon.h"
#include "Flourish/Api/Shader.h"
#include "Flourish/Api/Buffer.h"

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

    enum class DepthComparison
    {
        None = 0,
        Equal,
        NotEqual,
        Less,
        LessOrEqual,
        Greater,
        GreaterOrEqual,
        AlwaysTrue,
        AlwaysFalse,
        Auto
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

    struct DepthConfiguration
    {
        bool DepthTest = true;
        bool DepthWrite = true;
        DepthComparison CompareOperation = DepthComparison::Auto;
    };

    struct GraphicsPipelineCreateInfo
    {
        PipelineShader VertexShader;
        PipelineShader FragmentShader;

        bool VertexInput;
        VertexTopology VertexTopology;
        BufferLayout VertexLayout;

        // One state is required per output attachment
        std::vector<AttachmentBlendState> BlendStates; 

        DepthConfiguration DepthConfig;
        CullMode CullMode;
        WindingOrder WindingOrder;
       
        // If empty, will try and create for all subpasses
        std::vector<u32> CompatibleSubpasses;

        std::vector<AccessFlagsOverride> AccessOverrides;
    };

    class Texture;
    class ResourceSet;
    struct ResourceSetCreateInfo;
    class GraphicsPipeline
    {
    public:
        GraphicsPipeline(const GraphicsPipelineCreateInfo& createInfo)
            : m_Info(createInfo)
        {
            FL_ASSERT(createInfo.VertexShader.Shader && createInfo.FragmentShader.Shader, "Must specify both vertex and fragment shaders");
        }
        virtual ~GraphicsPipeline() = default;

        // Run when shaders potentially have been reloaded and the pipeline needs
        // to be recreated. Handled automatically in encoders. Returns true if
        // pipeline needed to be recreated.
        virtual bool ValidateShaders() = 0;

        // TS
        // NOTE: Try to keep binding & set indices as low as possible
        virtual std::shared_ptr<ResourceSet> CreateResourceSet(u32 setIndex, const ResourceSetCreateInfo& createInfo) = 0;
        
        // TS
        inline Shader* GetVertexShader() const { return m_Info.VertexShader.Shader.get(); }
        inline Shader* GetFragmentShader() const { return m_Info.FragmentShader.Shader.get(); }
        inline VertexTopology GetVertexTopology() const { return m_Info.VertexTopology; }
        inline CullMode GetCullMode() const { return m_Info.CullMode; }
        inline WindingOrder GetWindingOrder() const { return m_Info.WindingOrder; }
        inline const DepthConfiguration& GetDepthConfig() const { return m_Info.DepthConfig; }
        inline u32 GetVertexLayoutStride() const { return m_Info.VertexLayout.GetCalculatedStride(); }
        inline const auto& GetBlendStates() const { return m_Info.BlendStates; }

    protected:
        GraphicsPipelineCreateInfo m_Info;
    };
}
