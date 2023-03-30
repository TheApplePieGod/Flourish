#pragma once

#include "Flourish/Api/Framebuffer.h"

namespace Flourish
{
    struct DescriptorSetCreateInfo
    {
        u32 SetIndex;
    };

    class DescriptorSet
    {
    public:
        DescriptorSet(const DescriptorSetCreateInfo& createInfo)
            : m_Info(createInfo)
        {}
        virtual ~DescriptorSet() = default;

        virtual void BindBuffer(u32 bindingIndex, Buffer *buffer, u32 bufferOffset, u32 dynamicOffset, u32 elementCount) = 0;
        virtual void BindTexture(u32 bindingIndex, Texture *texture) = 0;
        virtual void BindTextureLayer(u32 bindingIndex, Texture *texture, u32 layerIndex, u32 mipLevel) = 0;
        virtual void BindSubpassInput(u32 bindingIndex, SubpassAttachment attachment) = 0;
        virtual void FlushBindings() = 0;

    protected:
        DescriptorSetCreateInfo m_Info;
    };
}
