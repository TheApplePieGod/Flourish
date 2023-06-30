#pragma once

#include "Flourish/Api/Shader.h"

namespace Flourish
{
    struct AccessFlagsOverride
    {
        u32 SetIndex;
        u32 BindingIndex;
        ShaderType Flags;
    };

    enum class SpecializationConstantType
    {
        Int = 0,
        UInt,
        Float,
        Bool
    };

    struct SpecializationConstant
    {
        SpecializationConstantType Type;
        u32 ConstantId;
        union
        {
            int Int;
            u32 UInt;
            float Float;
            b32 Bool;
        } Data;
    };

    struct PipelineShader
    {
        std::shared_ptr<Shader> Shader;
        std::vector<SpecializationConstant> Specializations;
    };
}
