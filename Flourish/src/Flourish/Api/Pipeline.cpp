#include "flpch.h"
#include "Pipeline.h"

#include "Flourish/Api/Context.h"

namespace Flourish
{
    void Pipeline::SortReflectionData()
    {
        std::sort(m_ProgramReflectionData.begin(), m_ProgramReflectionData.end(), [](const ReflectionDataElement& a, const ReflectionDataElement& b)
        {
            return a.BindingIndex < b.BindingIndex;
        });
    }
}