#pragma once

#include "Flourish/Api/Texture.h"
#include "Flourish/Api/GraphicsPipeline.h"

namespace Flourish
{
    enum class MsaaSampleCount
    {
        None = 1,
        Two, Four, Eight, Sixteen, Thirtytwo, Sixtyfour,
        Max = Sixtyfour
    };

    enum class AttachmentInitialization
    {
        None = 0, // Do nothing. Attachment contents are undefined
        Preserve, // Keep previously rendered data
        Clear     // Clear to a value specified in the framebuffer
    };

    enum class SubpassAttachmentType
    {
        None = 0,
        Color, Depth
    };

    struct SubpassAttachment
    {
        SubpassAttachmentType Type;
        u32 AttachmentIndex;
    };

    struct Subpass
    {
        std::vector<SubpassAttachment> InputAttachments;
        std::vector<SubpassAttachment> OutputAttachments;
    };

    struct ColorAttachmentSpec
    {
        ColorFormat Format;
        AttachmentInitialization Initialization = AttachmentInitialization::Clear;
        bool SupportComputeImages = false;
    };

    struct DepthAttachmentSpec
    {
        ColorFormat Format;
        AttachmentInitialization Initialization = AttachmentInitialization::Clear;
    };

    struct RenderPassCreateInfo
    {
        std::vector<ColorAttachmentSpec> ColorAttachments;
        std::vector<DepthAttachmentSpec> DepthAttachments;
        std::vector<Subpass> Subpasses; // One subpass is always required
        MsaaSampleCount SampleCount = MsaaSampleCount::None; // Will be clamped to device max supported sample count
    };

    class RenderPass
    {
    public:
        RenderPass(const RenderPassCreateInfo& createInfo)
            : m_Info(createInfo)
        {}
        virtual ~RenderPass() = default;
        
        // TS
        std::shared_ptr<GraphicsPipeline> CreatePipeline(std::string_view name, const GraphicsPipelineCreateInfo& createInfo);
        std::shared_ptr<GraphicsPipeline> GetPipeline(std::string_view name);
        u32 GetColorAttachmentCount(u32 subpassIndex);

        // TS
        inline MsaaSampleCount GetSampleCount() const { return m_Info.SampleCount; }
        inline const auto& GetSubpasses() const { return m_Info.Subpasses; }
        inline const auto& GetColorAttachment(u32 index) const { return m_Info.ColorAttachments[index]; }
        inline const auto& GetDepthAttachment(u32 index) const { return m_Info.DepthAttachments[index]; }

    public:
        // TS
        static std::shared_ptr<RenderPass> Create(const RenderPassCreateInfo& createInfo);
    
    protected:
        virtual std::shared_ptr<GraphicsPipeline> CreatePipeline(const GraphicsPipelineCreateInfo& createInfo) = 0; 

    protected:
        RenderPassCreateInfo m_Info;
        std::unordered_map<std::string, std::shared_ptr<GraphicsPipeline>> m_Pipelines;
        std::shared_mutex m_PipelineLock;
    };
}
