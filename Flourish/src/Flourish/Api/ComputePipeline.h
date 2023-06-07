#pragma once

#include "Flourish/Api/Shader.h"

namespace Flourish
{
    struct ComputePipelineCreateInfo
    {
        std::shared_ptr<Shader> ComputeShader;
    };

    class DescriptorSet;
    struct DescriptorSetCreateInfo;
    class ComputePipeline
    {
    public:
        ComputePipeline(const ComputePipelineCreateInfo& createInfo)
            : m_Info(createInfo)
        {
            FL_ASSERT(createInfo.ComputeShader, "Must specify a compute shader asset");
        }
        virtual ~ComputePipeline() = default;
        
        // TS
        // NOTE: Try to keep binding & set indices as low as possible
        virtual std::shared_ptr<DescriptorSet> CreateDescriptorSet(u32 setIndex, const DescriptorSetCreateInfo& createInfo) = 0;

        inline const Shader* GetComputeShader() const { return m_Info.ComputeShader.get(); }
        
    public:
        // TS
        static std::shared_ptr<ComputePipeline> Create(const ComputePipelineCreateInfo& createInfo);

    protected:
        ComputePipelineCreateInfo m_Info;
    };
}
