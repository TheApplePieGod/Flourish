#include "flpch.h"
#include "Synchronization.h"

#include "Flourish/Backends/Vulkan/Context.h"
#include "vulkan/vulkan_core.h"

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

    VkEvent Synchronization::CreateEvent()
    {
        VkEventCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_EVENT_CREATE_INFO;
        createInfo.flags = VK_EVENT_CREATE_DEVICE_ONLY_BIT;

        VkEvent event;
        vkCreateEvent(Context::Devices().Device(), &createInfo, NULL, &event);

        return event;
    }

    void Synchronization::WaitForFences(const VkFence* fences, u32 count)
    {
        FL_VK_ENSURE_RESULT(
            vkWaitForFences(Context::Devices().Device(), count, fences, true, UINT64_MAX),
            "WaitForFences"
        );
    }

    void Synchronization::ResetFences(const VkFence* fences, u32 count)
    {
        vkResetFences(Context::Devices().Device(), count, fences);
    }

    bool Synchronization::IsFenceSignalled(VkFence fence)
    {
        return vkGetFenceStatus(Context::Devices().Device(), fence) == VK_SUCCESS;
    }
}
