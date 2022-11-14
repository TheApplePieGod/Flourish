#pragma once

#include "Flourish/Api/Shader.h"
#include "Flourish/Api/Buffer.h"

namespace Flourish
{
    enum class PipelineType
    {
        None = 0,
        Graphics, 
        Compute
    };

    class Pipeline
    {
    public:
        virtual ~Pipeline() = default;

        inline const auto& GetReflectionData() const { return m_ProgramReflectionData; }

    protected:
        void SortReflectionData();

    protected:
        std::vector<ReflectionDataElement> m_ProgramReflectionData;
    };
}