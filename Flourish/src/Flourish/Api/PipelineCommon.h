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
}
