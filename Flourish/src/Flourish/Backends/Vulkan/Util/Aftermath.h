#pragma once

#include "Flourish/Backends/Vulkan/Util/Common.h"

namespace Flourish::Vulkan
{
    class Aftermath
    {
    public:
        static void Initialize();
        static void Shutdown();
    };
}
