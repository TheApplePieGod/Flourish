#pragma once

#include "Flourish/Api/RenderPass.h"
#include "Flourish/Api/Texture.h"

namespace Flourish
{
    struct FramebufferColorAttachment
    {
        // Required
        std::array<float, 4> ClearColor = { 0.f, 0.f, 0.f, 0.f };

        // Specify a texture to be used as a render target
        std::shared_ptr<Texture> Texture = nullptr;
        u32 LayerIndex = 0;
        u32 MipLevel = 0;
    };

    struct FramebufferDepthAttachment
    {
        // Specify a texture to be used as a render target
        std::shared_ptr<Texture> Texture = nullptr;
        u32 LayerIndex = 0;
        u32 MipLevel = 0;
    };

    struct FramebufferCreateInfo
    {
        std::shared_ptr<RenderPass> RenderPass;
        std::vector<FramebufferColorAttachment> ColorAttachments;
        std::vector<FramebufferDepthAttachment> DepthAttachments;
        u32 Width = 0;
        u32 Height = 0;
    };

    class Framebuffer
    {        
    public:
        Framebuffer(const FramebufferCreateInfo& createInfo)
            : m_Info(createInfo)
        {}
        virtual ~Framebuffer() = default;
        
        // TS
        inline u32 GetWidth() { return m_Info.Width; }
        inline u32 GetHeight() { return m_Info.Height; }
        inline RenderPass* GetRenderPass() const { return m_Info.RenderPass.get(); }
        inline const auto& GetColorAttachments() const { return m_Info.ColorAttachments; }
        inline const auto& GetDepthAttachments() const { return m_Info.DepthAttachments; }
        
    public:
        // TS
        static std::shared_ptr<Framebuffer> Create(const FramebufferCreateInfo& createInfo);

    protected:
        FramebufferCreateInfo m_Info;
    };
}
