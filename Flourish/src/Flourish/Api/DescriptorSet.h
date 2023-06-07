#pragma once

#include "Flourish/Api/Framebuffer.h"

namespace Flourish
{
    namespace DescriptorSetPipelineCompatabilityEnum
    {
        enum Value : u8
        {
            None = 0,
            Graphics = 1 << 0,
            Compute = 1 << 1,
        };
    }
    typedef DescriptorSetPipelineCompatabilityEnum::Value DescriptorSetPipelineCompatabilityFlags;
    typedef u8 DescriptorSetPipelineCompatability;

    enum class DescriptorSetWritability : u8
    {
        _DynamicData = 1,
        _FrameWrite = (1 << 1),
        _MultiWrite = (1 << 2),

        OnceStaticData = 0,
        OnceDynamicData = _DynamicData,
        PerFrame = _DynamicData | _FrameWrite,
        MultiPerFrame = _DynamicData | _FrameWrite | _MultiWrite,
    };

    struct DescriptorSetCreateInfo
    {
        DescriptorSetWritability Writability;
        bool StoreBindingReferences = false;
    };

    // TODO: Change name to resourcebindings or something more descriptive
    class DescriptorSet
    {
    public:
        DescriptorSet(const DescriptorSetCreateInfo& createInfo, DescriptorSetPipelineCompatability compatability)
            : m_Info(createInfo), m_Compatability(compatability)
        {}
        virtual ~DescriptorSet() = default;

        // Offset and elementcount in element size not bytes
        virtual void BindBuffer(u32 bindingIndex, const std::shared_ptr<Buffer>& buffer, u32 bufferOffset, u32 elementCount) = 0;
        virtual void BindTexture(u32 bindingIndex, const std::shared_ptr<Texture>& texture) = 0;
        virtual void BindTextureLayer(u32 bindingIndex, const std::shared_ptr<Texture>& texture, u32 layerIndex, u32 mipLevel) = 0;
        virtual void BindSubpassInput(u32 bindingIndex, const std::shared_ptr<Framebuffer>& framebuffer, SubpassAttachment attachment) = 0;
        virtual void BindBuffer(u32 bindingIndex, const Buffer* buffer, u32 bufferOffset, u32 elementCount) = 0;
        virtual void BindTexture(u32 bindingIndex, const Texture* texture) = 0;
        virtual void BindTextureLayer(u32 bindingIndex, const Texture* texture, u32 layerIndex, u32 mipLevel) = 0;
        virtual void BindSubpassInput(u32 bindingIndex, const Framebuffer* framebuffer, SubpassAttachment attachment) = 0;

        // Cannot flush until all bindings are bound
        virtual void FlushBindings() = 0;

        // TS
        inline DescriptorSetPipelineCompatability GetPipelineCompatability() const { return m_Compatability; }

    protected:
        DescriptorSetCreateInfo m_Info;
        DescriptorSetPipelineCompatability m_Compatability;
    };
}
