#include "flpch.h"
#include "ComputePipeline.h"

#include "Flourish/Api/Context.h"

namespace Flourish
{
    void ComputePipeline::ConsolidateReflectionData()
    {
        m_ProgramReflectionData.clear();
        m_ProgramReflectionData.insert(
            m_ProgramReflectionData.end(),
            m_Info.ComputeShader->GetReflectionData().begin(),
            m_Info.ComputeShader->GetReflectionData().end()
        );

        SortReflectionData();
    }
}