#pragma once

#include "Flourish/Api/Pipeline.h"

namespace Flourish
{
    struct ComputePipelineCreateInfo
    {
        std::shared_ptr<Shader> ComputeShader;
    };

    class ComputePipeline : public Pipeline
    {
    public:
        ComputePipeline(const ComputePipelineCreateInfo& createInfo)
            : m_Info(createInfo)
        {
            FL_ASSERT(createInfo.ComputeShader, "Must specify a compute shader asset");
            ConsolidateReflectionData();
        }
        virtual ~ComputePipeline() = default;
        
    public:
        // TS
        static std::shared_ptr<ComputePipeline> Create(const ComputePipelineCreateInfo& createInfo);

    protected:
        ComputePipelineCreateInfo m_Info;

    private:
        void ConsolidateReflectionData();
    };
}