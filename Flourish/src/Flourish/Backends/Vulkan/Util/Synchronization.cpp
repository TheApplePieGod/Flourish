#include "flpch.h"
#include "Synchronization.h"

#include "Flourish/Backends/Vulkan/Util/Context.h"

namespace Flourish::Vulkan
{
    VkSemaphore Synchronization::CreateTimelineSemaphore(u32 initialValue)
    {
        VkSemaphoreTypeCreateInfo timelineCreateInfo{};
        timelineCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
        timelineCreateInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
        timelineCreateInfo.initialValue = initialValue;

        VkSemaphoreCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        createInfo.pNext = &timelineCreateInfo;

        VkSemaphore timelineSemaphore;
        vkCreateSemaphore(Context::Devices().Device(), &createInfo, NULL, &timelineSemaphore);

        return timelineSemaphore;
    }

    VkSemaphore Synchronization::CreateSemaphore()
    {
        VkSemaphoreCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkSemaphore semaphore;
        vkCreateSemaphore(Context::Devices().Device(), &createInfo, NULL, &semaphore);

        return semaphore;
    }

    VkFence Synchronization::CreateFence()
    {
        VkFenceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        createInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        VkFence fence;
        vkCreateFence(Context::Devices().Device(), &createInfo, NULL, &fence);

        return fence;
    }
}