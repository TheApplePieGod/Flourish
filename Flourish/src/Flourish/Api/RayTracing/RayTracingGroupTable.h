#pragma once

#include "Flourish/Api/Buffer.h"

namespace Flourish
{
    class RayTracingPipeline;
    struct RayTracingGroupTableCreateInfo
    {
        std::shared_ptr<RayTracingPipeline> Pipeline;
        u32 MaxHitEntries = 10;
        u32 MaxMissEntries = 10;
        u32 MaxCallableEntries = 10;
    };

    class RayTracingGroupTable
    {
    public:
        RayTracingGroupTable(const RayTracingGroupTableCreateInfo& createInfo)
            : m_Info(createInfo)
        {}
        virtual ~RayTracingGroupTable() = default;

        virtual void BindRayGenGroup(u32 groupIndex) = 0;
        virtual void BindHitGroup(u32 groupIndex, u32 offset) = 0;
        virtual void BindMissGroup(u32 groupIndex, u32 offset) = 0;
        virtual void BindCallableGroup(u32 groupIndex, u32 offset) = 0;

    public:
        static std::shared_ptr<RayTracingGroupTable> Create(const RayTracingGroupTableCreateInfo& createInfo);

    protected:
        RayTracingGroupTableCreateInfo m_Info;
    };
}
