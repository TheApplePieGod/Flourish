#pragma once

#include "Flourish/Api/RenderPass.h"
#include "Flourish/Api/Texture.h"

namespace Flourish
{
    struct FramebufferColorAttachment
    {
        // Required
        float ClearColor[4];

        // Specify a texture to be used as a render target
        std::shared_ptr<Texture> Texture = nullptr;
        u32 LayerIndex = 0;
        u32 MipLevel = 0;
    };

    struct FramebufferDepthAttachment
    {};

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

    public:
        // TS
        static std::shared_ptr<Framebuffer> Create(const FramebufferCreateInfo& createInfo);

    protected:
        FramebufferCreateInfo m_Info;
    };
}