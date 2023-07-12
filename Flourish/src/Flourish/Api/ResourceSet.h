#pragma once

#include "Flourish/Api/Framebuffer.h"

namespace Flourish
{
    namespace ResourceSetPipelineCompatabilityEnum
    {
        enum Value : u8
        {
            None = 0,
            Graphics = (1 << 0),
            Compute = (1 << 1),
            RayTracing = (1 << 2),
            All = 255
        };
    }
    typedef ResourceSetPipelineCompatabilityEnum::Value ResourceSetPipelineCompatabilityFlags;
    typedef u8 ResourceSetPipelineCompatability;

    enum class ResourceSetWritability : u8
    {
        _DynamicData = (1 << 0),
        _FrameWrite = (1 << 1),
        _MultiWrite = (1 << 2),

        OnceStaticData = 0,
        OnceDynamicData = _DynamicData,
        PerFrame = _DynamicData | _FrameWrite,
        MultiPerFrame = _DynamicData | _FrameWrite | _MultiWrite,
    };

    struct ResourceSetCreateInfo
    {
        ResourceSetWritability Writability;
        bool StoreBindingReferences = false;
    };

    class AccelerationStructure;
    class ResourceSet
    {
    public:
        ResourceSet(const ResourceSetCreateInfo& createInfo, ResourceSetPipelineCompatability compatability)
            : m_Info(createInfo), m_Compatability(compatability)
        {}
        virtual ~ResourceSet() = default;

        // Offset and elementcount in element size not bytes
        virtual void BindBuffer(u32 bindingIndex, const std::shared_ptr<Buffer>& buffer, u32 bufferOffset, u32 elementCount) = 0;
        virtual void BindTexture(u32 bindingIndex, const std::shared_ptr<Texture>& texture, u32 arrayIndex = 0) = 0;
        virtual void BindTextureLayer(u32 bindingIndex, const std::shared_ptr<Texture>& texture, u32 layerIndex, u32 mipLevel, u32 arrayIndex = 0) = 0;
        virtual void BindSubpassInput(u32 bindingIndex, const std::shared_ptr<Framebuffer>& framebuffer, SubpassAttachment attachment) = 0;
        virtual void BindAccelerationStructure(u32 bindingIndex, const std::shared_ptr<AccelerationStructure>& accelStruct) = 0;

        virtual void BindBuffer(u32 bindingIndex, const Buffer* buffer, u32 bufferOffset, u32 elementCount) = 0;
        virtual void BindTexture(u32 bindingIndex, const Texture* texture, u32 arrayIndex = 0) = 0;
        virtual void BindTextureLayer(u32 bindingIndex, const Texture* texture, u32 layerIndex, u32 mipLevel, u32 arrayIndex = 0) = 0;
        virtual void BindSubpassInput(u32 bindingIndex, const Framebuffer* framebuffer, SubpassAttachment attachment) = 0;
        virtual void BindAccelerationStructure(u32 bindingIndex, const AccelerationStructure* accelStruct) = 0;

        // Cannot flush until all bindings are bound
        virtual void FlushBindings() = 0;

        // TS
        inline ResourceSetPipelineCompatability GetPipelineCompatability() const { return m_Compatability; }

    protected:
        ResourceSetCreateInfo m_Info;
        ResourceSetPipelineCompatability m_Compatability;
    };
}
