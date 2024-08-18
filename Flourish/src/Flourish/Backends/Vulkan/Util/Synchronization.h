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
        static VkEvent CreateEvent();
        
        // TS
        static void WaitForFences(const VkFence* fences, u32 count);
        static void ResetFences(const VkFence* fences, u32 count);
        static bool IsFenceSignalled(VkFence fence);
    };
}
