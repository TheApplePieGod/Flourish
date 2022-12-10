#include "flpch.h"
#include "Devices.h"

#include "Flourish/Backends/Vulkan/Util/Swapchain.h"
#include "Flourish/Backends/Vulkan/Util/Context.h"

namespace Flourish::Vulkan
{
    void Devices::Initialize(const ContextInitializeInfo& initInfo)
    {
        VkInstance instance = Context::Instance();

        // Required physical device extensions
        std::vector<const char*> deviceExtensions = {
            #ifdef FL_PLATFORM_MACOS
                "VK_KHR_portability_subset",
            #endif
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
            VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME
        };

        // Get devices
        u32 deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
        FL_CRASH_ASSERT(deviceCount > 0, "No graphics devices were found while initializing");

        // Find first compatible device
        for (auto device : devices)
        {
            if (CheckDeviceCompatability(device, deviceExtensions))
            {
                m_PhysicalDevice = device;

                vkGetPhysicalDeviceProperties(m_PhysicalDevice, &m_PhysicalDeviceProperties);
                m_DeviceMaxSampleCount = GetMaxSampleCount();

                break;
            }
        }
        FL_CRASH_ASSERT(m_PhysicalDevice, "Unable to find a compatible gpu while initializing");

        PopulateOptionalExtensions(deviceExtensions, initInfo);

        // Get the max amount of queues we can/need to create for each family
        QueueFamilyIndices indices = Queues::GetQueueFamilies(m_PhysicalDevice);
        std::vector<float> queuePriorities;
        std::unordered_map<u32, u32> uniqueFamilies;
        u32 bufferCount = Flourish::Context::FrameBufferCount();
        queuePriorities.reserve(4 * bufferCount);
        uniqueFamilies[indices.PresentFamily.value()] = std::min(indices.PresentQueueCount, bufferCount);
        uniqueFamilies[indices.GraphicsFamily.value()] = std::min(indices.GraphicsQueueCount, bufferCount);
        uniqueFamilies[indices.ComputeFamily.value()] = std::min(indices.ComputeQueueCount, bufferCount);
        uniqueFamilies[indices.TransferFamily.value()] = std::min(indices.TransferQueueCount, bufferCount);

        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        for (auto& pair : uniqueFamilies)
        {
            VkDeviceQueueCreateInfo queueCreateInfo{};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = pair.first;
            queueCreateInfo.queueCount = pair.second;
            queueCreateInfo.pQueuePriorities = queuePriorities.data() + queuePriorities.size();
            queueCreateInfos.push_back(queueCreateInfo);

            for (u32 i = 0; i < pair.second; i++)
                queuePriorities.push_back(1.0);
        }

        VkPhysicalDeviceFeatures deviceFeatures{};
        PopulateDeviceFeatures(deviceFeatures, initInfo);

        VkPhysicalDeviceTimelineSemaphoreFeatures timelineFeatures{};
        timelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES;
        timelineFeatures.timelineSemaphore = true;
        
        // Create device
        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.pNext = &timelineFeatures;
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
        createInfo.queueCreateInfoCount = static_cast<u32>(queueCreateInfos.size());
        createInfo.pEnabledFeatures = &deviceFeatures;
        createInfo.enabledExtensionCount = static_cast<u32>(deviceExtensions.size());
        createInfo.ppEnabledExtensionNames = deviceExtensions.data();
        #if FL_DEBUG
            auto validationLayers = Context::ValidationLayers();
            createInfo.enabledLayerCount = static_cast<u32>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();
        #else
            createInfo.enabledLayerCount = 0;
        #endif

        FL_VK_ENSURE_RESULT(vkCreateDevice(m_PhysicalDevice, &createInfo, nullptr, &m_Device));

        // Load all device functions for this device. This will have to change
        // if we ever support multiple devices.
        volkLoadDevice(m_Device);
    }

    void Devices::Shutdown()
    {
        vkDestroyDevice(m_Device, nullptr);
        m_Device = nullptr;
        m_PhysicalDevice = nullptr;
    }

    bool Devices::CheckDeviceCompatability(VkPhysicalDevice device, const std::vector<const char*>& extensions)
    {
        QueueFamilyIndices indices = Queues::GetQueueFamilies(device);

        // Get hardware extension support
        u32 supportedExtensionCount = 0;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &supportedExtensionCount, nullptr);
        std::vector<VkExtensionProperties> supportedExtensions(supportedExtensionCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &supportedExtensionCount, supportedExtensions.data());

        // Ensure extension compatability
        for (auto extension : extensions)
            if (!Common::SupportsExtension(supportedExtensions, extension))
                return false;
        
        return indices.IsComplete();
    }

    void Devices::PopulateOptionalExtensions(std::vector<const char*>& extensions, const ContextInitializeInfo& initInfo)
    {
        // Get hardware extension support
        u32 supportedExtensionCount = 0;
        vkEnumerateDeviceExtensionProperties(m_PhysicalDevice, nullptr, &supportedExtensionCount, nullptr);
        std::vector<VkExtensionProperties> supportedExtensions(supportedExtensionCount);
        vkEnumerateDeviceExtensionProperties(m_PhysicalDevice, nullptr, &supportedExtensionCount, supportedExtensions.data());
        
        if (initInfo.RequestedFeatures.SamplerMinMax &&
            Common::SupportsExtension(supportedExtensions, VK_EXT_SAMPLER_FILTER_MINMAX_EXTENSION_NAME))
        {
            extensions.push_back(VK_EXT_SAMPLER_FILTER_MINMAX_EXTENSION_NAME);
            Flourish::Context::FeatureTable().SamplerMinMax = true;
        }
    }

    void Devices::PopulateDeviceFeatures(VkPhysicalDeviceFeatures& features, const ContextInitializeInfo& initInfo)
    {
        VkPhysicalDeviceFeatures supported;
        vkGetPhysicalDeviceFeatures(m_PhysicalDevice, &supported);
        
        if (initInfo.RequestedFeatures.SamplerAnisotropy && supported.samplerAnisotropy)
        {
            features.samplerAnisotropy = true;
            Flourish::Context::FeatureTable().SamplerAnisotropy = true;
        }
        if (initInfo.RequestedFeatures.IndependentBlend && supported.independentBlend)
        {
            features.independentBlend = true;
            Flourish::Context::FeatureTable().IndependentBlend = true;
        }
        if (initInfo.RequestedFeatures.WideLines && supported.wideLines)
        {
            features.wideLines = true;
            Flourish::Context::FeatureTable().WideLines = true;
        }
    }

    VkSampleCountFlagBits Devices::GetMaxSampleCount()
    {
        VkSampleCountFlags counts = m_PhysicalDeviceProperties.limits.framebufferColorSampleCounts & m_PhysicalDeviceProperties.limits.framebufferDepthSampleCounts;

        if (counts & VK_SAMPLE_COUNT_64_BIT) { return VK_SAMPLE_COUNT_64_BIT; }
        if (counts & VK_SAMPLE_COUNT_32_BIT) { return VK_SAMPLE_COUNT_32_BIT; }
        if (counts & VK_SAMPLE_COUNT_16_BIT) { return VK_SAMPLE_COUNT_16_BIT; }
        if (counts & VK_SAMPLE_COUNT_8_BIT) { return VK_SAMPLE_COUNT_8_BIT; }
        if (counts & VK_SAMPLE_COUNT_4_BIT) { return VK_SAMPLE_COUNT_4_BIT; }
        if (counts & VK_SAMPLE_COUNT_2_BIT) { return VK_SAMPLE_COUNT_2_BIT; }

        return VK_SAMPLE_COUNT_1_BIT;
    }
}