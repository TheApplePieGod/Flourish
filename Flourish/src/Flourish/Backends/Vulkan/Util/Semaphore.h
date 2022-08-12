#pragma once

#include "Flourish/Backends/Vulkan/Util/Common.h"

namespace Flourish::Vulkan
{
    struct Semaphore
    {
        // TS
        static VkSemaphore CreateTimelineSemaphore(u32 initialValue);
    };
}