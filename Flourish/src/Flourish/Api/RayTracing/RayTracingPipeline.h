#pragma once

#include "Flourish/Api/Shader.h"

namespace Flourish
{
    struct RayTracingShaderGroup
    {
        u32 GeneralShader = 0;
        u32 IntersectionShader = 0;
        u32 ClosestHitShader = 0;
        u32 AnyHitShader = 0;
    };

    struct RayTracingPipelineCreateInfo
    {
        std::vector<std::shared_ptr<Shader>> Shaders;
        std::vector<RayTracingShaderGroup> Groups;
        u32 MaxRayRecursionDepth = 1;
    };

    class ResourceSet;
    struct ResourceSetCreateInfo;
    class RayTracingPipeline
    {
    public:
        RayTracingPipeline(const RayTracingPipelineCreateInfo& createInfo)
            : m_Info(createInfo)
        {
            FL_ASSERT(!createInfo.Shaders.empty(), "Must specify at least one shader");
            FL_ASSERT(!createInfo.Groups.empty(), "Must specify at least one group");
        }
        virtual ~RayTracingPipeline() = default;
        
        // TS
        // NOTE: Try to keep binding & set indices as low as possible
        virtual std::shared_ptr<ResourceSet> CreateResourceSet(u32 setIndex, const ResourceSetCreateInfo& createInfo) = 0;
        
    public:
        // TS
        static std::shared_ptr<RayTracingPipeline> Create(const RayTracingPipelineCreateInfo& createInfo);

    protected:
        RayTracingPipelineCreateInfo m_Info;
    };
}
