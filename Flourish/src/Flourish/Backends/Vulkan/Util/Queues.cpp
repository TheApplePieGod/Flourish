#include "flpch.h"
#include "Queues.h"

#include "Flourish/Backends/Vulkan/Util/Context.h"

namespace Flourish::Vulkan
{
    void Queues::Initialize()
    {
        auto indices = GetQueueFamilies(Context::Devices().PhysicalDevice());

        m_GraphicsQueueIndex = indices.GraphicsFamily.value();
        m_PresentQueueIndex = indices.PresentFamily.value();
        m_ComputeQueueIndex = indices.ComputeFamily.value();
        m_TransferQueueIndex = indices.TransferFamily.value();

        for (u32 i = 0; i < Flourish::Context::GetFrameBufferCount(); i++)
        {
            vkGetDeviceQueue(Context::Devices().Device(), m_PresentQueueIndex, std::min(i, indices.PresentQueueCount - 1), &m_PresentQueues[i]);
            vkGetDeviceQueue(Context::Devices().Device(), m_GraphicsQueueIndex, std::min(i, indices.GraphicsQueueCount - 1), &m_GraphicsQueues[i]);
            vkGetDeviceQueue(Context::Devices().Device(), m_ComputeQueueIndex, std::min(i, indices.ComputeQueueCount - 1), &m_ComputeQueues[i]);
            vkGetDeviceQueue(Context::Devices().Device(), m_TransferQueueIndex, std::min(i, indices.TransferQueueCount - 1), &m_TransferQueues[i]);
        }
    }

    void Queues::Shutdown()
    {

    }
    
    QueueFamilyIndices Queues::GetQueueFamilies(VkPhysicalDevice device)
    {
        QueueFamilyIndices indices;

        u32 supportedQueueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &supportedQueueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> supportedQueueFamilies(supportedQueueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &supportedQueueFamilyCount, supportedQueueFamilies.data());

        // Pass over the supported families and populate indices with the first available option
        int i = 0;
        for (const auto& family : supportedQueueFamilies)
        {
            if (indices.IsComplete()) break;

            if (family.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                indices.GraphicsFamily = i;
                indices.GraphicsQueueCount = family.queueCount;
            }

            if (family.queueFlags & VK_QUEUE_COMPUTE_BIT)
            {
                indices.ComputeFamily = i;
                indices.ComputeQueueCount = family.queueCount;
            }

            if (family.queueFlags & VK_QUEUE_TRANSFER_BIT)
            {
                indices.TransferFamily = i;
                indices.TransferQueueCount = family.queueCount;
            }

            VkBool32 presentSupport = false;
            #ifdef FL_PLATFORM_WINDOWS
                presentSupport = vkGetPhysicalDeviceWin32PresentationSupportKHR(device, i);
            #elif defined(FL_PLATFORM_LINUX)
                // TODO: we need to figure out where these params come from
                // presentSupport = vkGetPhysicalDeviceXcbPresentationSupportKHR(device, i, ???, ???);
                presentSupport = true;
            #endif

            if (presentSupport)
            {
                indices.PresentFamily = i;
                indices.PresentQueueCount = family.queueCount;
            }
            
            i++;
        }

        i = 0;
        for (const auto& family : supportedQueueFamilies)
        {
            // Prioritize families that support compute but not graphics
            if (family.queueFlags & VK_QUEUE_COMPUTE_BIT && !(family.queueFlags & VK_QUEUE_GRAPHICS_BIT))
            {
                indices.ComputeFamily = i;
                indices.ComputeQueueCount = family.queueCount;
            }

            // Prioritize families that support transfer only
            if (family.queueFlags & VK_QUEUE_TRANSFER_BIT && !(family.queueFlags & VK_QUEUE_GRAPHICS_BIT) && !(family.queueFlags & VK_QUEUE_COMPUTE_BIT))
            {
                indices.TransferFamily = i;
                indices.TransferQueueCount = family.queueCount;
            }

            i++;
        }

        return indices;
    }
}