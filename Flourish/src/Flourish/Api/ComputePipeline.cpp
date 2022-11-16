#include "flpch.h"
#include "ComputePipeline.h"

#include "Flourish/Api/Context.h"
#include "Flourish/Backends/Vulkan/ComputePipeline.h"

namespace Flourish
{
    std::shared_ptr<ComputePipeline> ComputePipeline::Create(const ComputePipelineCreateInfo& createInfo)
    {
        FL_ASSERT(Context::BackendType() != BackendType::None, "Must initialize Context before creating a ComputePipeline");

        switch (Context::BackendType())
        {
            case BackendType::Vulkan: { return std::make_shared<Vulkan::ComputePipeline>(createInfo); }
        }

        FL_ASSERT(false, "ComputePipeline not supported for backend");
        return nullptr;
    }

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