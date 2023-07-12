#pragma once

#include "Flourish/Api/CommandEncoder.h"

namespace Flourish
{
    class Texture;
    class Buffer;
    class ComputePipeline;
    class ResourceSet;
    class RayTracingPipeline;
    class RayTracingGroupTable;
    class AccelerationStructure;
    class AccelerationStructureSceneBuildInfo;
    class AccelerationStructureNodeBuildInfo;
    class ComputeCommandEncoder : public CommandEncoder
    {
    public:
        ComputeCommandEncoder() = default;

        virtual void BindComputePipeline(ComputePipeline* pipeline) = 0;
        virtual void Dispatch(u32 x, u32 y, u32 z) = 0;
        virtual void DispatchIndirect(Buffer* buffer, u32 commandOffset) = 0;

        virtual void BindRayTracingPipeline(RayTracingPipeline* pipeline) = 0;
        virtual void TraceRays(RayTracingGroupTable* groupTable, u32 width, u32 height, u32 depth) = 0;

        virtual void RebuildAccelerationStructureScene(AccelerationStructure* accel, const AccelerationStructureSceneBuildInfo& buildInfo) = 0;
        virtual void RebuildAccelerationStructureNode(AccelerationStructure* accel, const AccelerationStructureNodeBuildInfo& buildInfo) = 0;

        // Bind -> Update -> Flush
        // Offset in bytes
        virtual void UpdateDynamicOffset(u32 setIndex, u32 bindingIndex, u32 offset) = 0;
        virtual void BindResourceSet(const ResourceSet* set, u32 setIndex) = 0;
        virtual void FlushResourceSet(u32 setIndex) = 0;
        virtual void PushConstants(u32 offset, u32 size, const void* data) = 0;
    };
}
