#include "flpch.h"
#include "Commands.h"

#include "Flourish/Backends/Vulkan/Util/Context.h"

namespace Flourish::Vulkan
{
    void Commands::Initialize()
    {
        auto device = Context::Devices().Device();
        
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = Context::Queues().GraphicsQueueIndex();
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; // allow resetting

        FL_VK_ENSURE_RESULT(vkCreateCommandPool(device, &poolInfo, nullptr, &m_GraphicsPool));
        
        poolInfo.queueFamilyIndex = Context::Queues().ComputeQueueIndex();

        FL_VK_ENSURE_RESULT(vkCreateCommandPool(device, &poolInfo, nullptr, &m_ComputePool));

        poolInfo.queueFamilyIndex = Context::Queues().TransferQueueIndex();

        FL_VK_ENSURE_RESULT(vkCreateCommandPool(device, &poolInfo, nullptr, &m_TransferPool));
    }

    void Commands::Shutdown()
    {
        auto device = Context::Devices().Device();

        vkDestroyCommandPool(device, m_GraphicsPool, nullptr);
        vkDestroyCommandPool(device, m_ComputePool, nullptr);
        vkDestroyCommandPool(device, m_TransferPool, nullptr);
    }
}