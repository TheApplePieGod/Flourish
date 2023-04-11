#pragma once

#include "Flourish/Api/CommandEncoder.h"

namespace Flourish
{
    class Texture;
    class Buffer;
    class ComputePipeline;
    class DescriptorSet;
    class ComputeCommandEncoder : public CommandEncoder
    {
    public:
        ComputeCommandEncoder() = default;

        virtual void BindPipeline(ComputePipeline* pipeline) = 0;
        virtual void Dispatch(u32 x, u32 y, u32 z) = 0;
        virtual void DispatchIndirect(Buffer* buffer, u32 commandOffset) = 0;

        // Bind -> Update -> Flush
        // Offset in bytes
        virtual void UpdateDynamicOffset(u32 setIndex, u32 bindingIndex, u32 offset) = 0;
        virtual void BindDescriptorSet(const DescriptorSet* set, u32 setIndex) = 0;
        virtual void FlushDescriptorSet(u32 setIndex) = 0;
    };
}
