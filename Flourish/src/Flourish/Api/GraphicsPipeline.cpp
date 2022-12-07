#include "flpch.h"
#include "GraphicsPipeline.h"

#include "Flourish/Api/Context.h"

namespace Flourish
{
    bool AttachmentBlendState::operator==(const AttachmentBlendState& other) const
    {
        return BlendEnable == other.BlendEnable &&
               SrcColorBlendFactor == other.SrcColorBlendFactor &&
               DstColorBlendFactor == other.DstColorBlendFactor &&
               SrcAlphaBlendFactor == other.SrcAlphaBlendFactor &&
               DstAlphaBlendFactor == other.DstAlphaBlendFactor &&
               ColorBlendOperation == other.ColorBlendOperation &&
               AlphaBlendOperation == other.AlphaBlendOperation;
    }

    void GraphicsPipeline::ConsolidateReflectionData()
    {
        m_ProgramReflectionData.clear();
        m_ProgramReflectionData.insert(
            m_ProgramReflectionData.end(),
            m_Info.VertexShader->GetReflectionData().begin(),
            m_Info.VertexShader->GetReflectionData().end()
        );

        for (auto& fragData : m_Info.FragmentShader->GetReflectionData())
        {
            bool add = true;
            for (auto& vertData : m_ProgramReflectionData)
            {
                if (fragData.BindingIndex == vertData.BindingIndex)
                {
                    FL_ASSERT(fragData.UniqueId != vertData.UniqueId, "Binding index must be unique for all shader resources");
                    vertData.AccessType = ShaderResourceAccessType::Both;
                    add = false;
                    break;
                }
            }
            if (add)
                m_ProgramReflectionData.push_back(fragData);
        }

        SortReflectionData();
    }
}