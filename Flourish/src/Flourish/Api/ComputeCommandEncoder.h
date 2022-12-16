#pragma once

#include "Flourish/Api/CommandEncoder.h"

namespace Flourish
{
    class ComputePipeline;
    class ComputeCommandEncoder : public CommandEncoder
    {
    public:
        ComputeCommandEncoder() = default;

        // All shader resources must be bound before drawing
        virtual void BindPipeline(ComputePipeline* pipeline) = 0;
        virtual void Dispatch(u32 x, u32 y, u32 z) = 0;
        virtual void DispatchIndirect(Buffer* buffer, u32 commandOffset) = 0;
        
        // Buffer offset refers to the element starting point in the buffer and dynamicOffset refers to a dynamic element offset
        virtual void BindPipelineBufferResource(u32 bindingIndex, Buffer* buffer, u32 bufferOffset, u32 dynamicOffset, u32 elementCount) = 0;
        virtual void BindPipelineTextureResource(u32 bindingIndex, Texture* texture) = 0;
        virtual void BindPipelineTextureLayerResource(u32 bindingIndex, Flourish::Texture* texture, u32 layerIndex, u32 mipLevel) = 0;
        virtual void FlushPipelineBindings() = 0;
    };
}