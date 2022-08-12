#include "flpch.h"
#include "Semaphore.h"

#include "Flourish/Backends/Vulkan/Util/Context.h"

namespace Flourish::Vulkan
{
    VkSemaphore Semaphore::CreateTimelineSemaphore(u32 initialValue)
    {
        VkSemaphoreTypeCreateInfo timelineCreateInfo;
        timelineCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
        timelineCreateInfo.pNext = NULL;
        timelineCreateInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
        timelineCreateInfo.initialValue = initialValue;

        VkSemaphoreCreateInfo createInfo;
        createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        createInfo.pNext = &timelineCreateInfo;
        createInfo.flags = 0;

        VkSemaphore timelineSemaphore;
        vkCreateSemaphore(Context::Devices().Device(), &createInfo, NULL, &timelineSemaphore);

        return timelineSemaphore;
    }
}