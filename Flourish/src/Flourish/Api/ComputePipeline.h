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

        inline u32 GetDispatchCountX() const { return m_DispatchCountX; }
        inline u32 GetDispatchCountY() const { return m_DispatchCountY; }
        inline u32 GetDispatchCountZ() const { return m_DispatchCountZ; }
        inline void SetDispatchCountX(u32 count) { m_DispatchCountX = count; }
        inline void SetDispatchCountY(u32 count) { m_DispatchCountY = count; }
        inline void SetDispatchCountZ(u32 count) { m_DispatchCountZ = count; }
        inline void SetDispatchCount(u32 x, u32 y, u32 z) { m_DispatchCountX = x; m_DispatchCountY = y; m_DispatchCountZ = z; }

    protected:
        ComputePipelineCreateInfo m_Info;
        u32 m_DispatchCountX = 1;
        u32 m_DispatchCountY = 1;
        u32 m_DispatchCountZ = 1;

    private:
        void ConsolidateReflectionData();
    };
}