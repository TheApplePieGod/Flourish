#pragma once

#include "Flourish/Api/Framebuffer.h"

namespace Flourish
{
    enum class DescriptorSetWritability : u8
    {
        DynamicData = 1,
        FrameWrite = (1 << 1),
        MultiWrite = (1 << 2),

        OnceStaticData = 0,
        OnceDynamicData = DynamicData,
        PerFrame = DynamicData | FrameWrite,
        MultiPerFrame = DynamicData | FrameWrite | MultiWrite,
    };

    struct DescriptorSetCreateInfo
    {
        u32 SetIndex; // TODO: move out of create info?
        DescriptorSetWritability Writability;
    };

    // TODO: Change name to resourcebindings or something more descriptive
    class DescriptorSet
    {
    public:
        DescriptorSet(const DescriptorSetCreateInfo& createInfo)
            : m_Info(createInfo)
        {}
        virtual ~DescriptorSet() = default;

        // Offset and elementcount in element size not bytes
        virtual void BindBuffer(u32 bindingIndex, const Buffer* buffer, u32 bufferOffset, u32 elementCount) = 0;
        virtual void BindTexture(u32 bindingIndex, const Texture* texture) = 0;
        virtual void BindTextureLayer(u32 bindingIndex, const Texture* texture, u32 layerIndex, u32 mipLevel) = 0;
        virtual void BindSubpassInput(u32 bindingIndex, const Framebuffer* framebuffer, SubpassAttachment attachment) = 0;

        // Cannot flush until all bindings are bound
        virtual void FlushBindings() = 0;

    protected:
        DescriptorSetCreateInfo m_Info;
    };
}
