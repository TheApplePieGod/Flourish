#pragma once

#include "Flourish/Api/PipelineCommon.h"
#include "Flourish/Api/Shader.h"

namespace Flourish
{
    struct ComputePipelineCreateInfo
    {
        PipelineShader Shader;

        std::vector<AccessFlagsOverride> AccessOverrides;
    };

    class ResourceSet;
    struct ResourceSetCreateInfo;
    class ComputePipeline
    {
    public:
        ComputePipeline(const ComputePipelineCreateInfo& createInfo)
            : m_Info(createInfo)
        {
            FL_ASSERT(createInfo.Shader.Shader, "Must specify a compute shader asset");
        }
        virtual ~ComputePipeline() = default;

        // Run when shaders potentially have been reloaded and the pipeline needs
        // to be recreated. Handled automatically in encoders. Returns true if
        // pipeline needed to be recreated.
        virtual bool ValidateShaders() = 0;
        
        // TS
        // NOTE: Try to keep binding & set indices as low as possible
        virtual std::shared_ptr<ResourceSet> CreateResourceSet(u32 setIndex, const ResourceSetCreateInfo& createInfo) = 0;

    public:
        // TS
        static std::shared_ptr<ComputePipeline> Create(const ComputePipelineCreateInfo& createInfo);

    protected:
        ComputePipelineCreateInfo m_Info;
    };
}
