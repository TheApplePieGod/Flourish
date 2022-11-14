#pragma once

#include "Flourish/Backends/Vulkan/Util/Common.h"

namespace Flourish::Vulkan
{
    struct SemaphorePool
    {
        std::vector<VkSemaphore> Semaphores;
        u32 FreeIndex;
    };

    struct Synchronization
    {
        // TS
        static VkSemaphore CreateTimelineSemaphore(u32 initialValue);
        static VkSemaphore CreateSemaphore();
        static VkFence CreateFence();
    };
}