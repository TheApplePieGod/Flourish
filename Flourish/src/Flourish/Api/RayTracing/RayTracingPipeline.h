#pragma once

#include "Flourish/Api/PipelineCommon.h"
#include "Flourish/Api/Shader.h"

namespace Flourish
{
    enum class RayTracingShaderGroupType : u8
    {
        RayGen = 0,
        Hit,
        Miss,
        Callable
    };

    struct RayTracingShaderGroup
    {
        int GeneralShader = -1;
        int IntersectionShader = -1;
        int ClosestHitShader = -1;
        int AnyHitShader = -1;
    };

    struct RayTracingPipelineCreateInfo
    {
        std::vector<std::shared_ptr<Shader>> Shaders;
        std::vector<RayTracingShaderGroup> Groups;
        u32 MaxRayRecursionDepth = 1;

        std::vector<AccessFlagsOverride> AccessOverrides;
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

        // TS
        inline const auto& GetGroups() const { return m_Info.Groups; }
        inline const auto& GetShaders() const { return m_Info.Shaders; }
        
    public:
        // TS
        static std::shared_ptr<RayTracingPipeline> Create(const RayTracingPipelineCreateInfo& createInfo);

    protected:
        RayTracingPipelineCreateInfo m_Info;
    };
}
