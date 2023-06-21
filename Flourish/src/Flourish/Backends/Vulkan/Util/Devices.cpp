#include "flpch.h"
#include "Devices.h"

#include "Flourish/Backends/Vulkan/Util/Swapchain.h"
#include "Flourish/Backends/Vulkan/Context.h"

namespace Flourish::Vulkan
{
    void Devices::Initialize(const ContextInitializeInfo& initInfo)
    {
        FL_LOG_TRACE("Vulkan device initialization begin");

        VkInstance instance = Context::Instance();

        // Required physical device extensions
        std::vector<const char*> deviceExtensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,

            // Not supported in MVK yet
            #if !defined(FL_PLATFORM_MACOS)
            VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME
            #endif
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

                FL_LOG_DEBUG("Compatible - yes. Using this device");
                FL_LOG_INFO("Found a compatible graphics device");
                DumpDeviceInfo(LogLevel::Info, m_PhysicalDeviceProperties);
                
                break;
            }
        }
        FL_CRASH_ASSERT(m_PhysicalDevice, "Unable to find a compatible graphics device while initializing");


        // TODO: clean this up / don't enable everything

        // Get hardware extension support
        u32 supportedExtensionCount = 0;
        vkEnumerateDeviceExtensionProperties(m_PhysicalDevice, nullptr, &supportedExtensionCount, nullptr);
        m_SupportedExtensions.resize(supportedExtensionCount);
        vkEnumerateDeviceExtensionProperties(m_PhysicalDevice, nullptr, &supportedExtensionCount, m_SupportedExtensions.data());

        VkPhysicalDeviceFeatures deviceFeatures{};
        PopulateFeatureTable(deviceFeatures, deviceExtensions, initInfo);
        PopulateDeviceProperties();
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

        // RT
        VkPhysicalDeviceAccelerationStructureFeaturesKHR accelFeatures{};
        accelFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
        accelFeatures.accelerationStructure = true;
        VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtFeatures{};
        rtFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
        rtFeatures.pNext = &accelFeatures;
        rtFeatures.rayTracingPipeline = true;

        VkPhysicalDeviceBufferDeviceAddressFeatures bufAddrFeatures{};
        bufAddrFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
        bufAddrFeatures.pNext = &rtFeatures;
        bufAddrFeatures.bufferDeviceAddress = true;

        VkPhysicalDeviceSynchronization2Features syncFeatures{};
        syncFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;
        syncFeatures.pNext = &bufAddrFeatures;
        syncFeatures.synchronization2 = true;

        VkPhysicalDeviceTimelineSemaphoreFeatures timelineFeatures{};
        timelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES;
        timelineFeatures.timelineSemaphore = true;
        #if defined(FL_PLATFORM_MACOS)
            VkPhysicalDevicePortabilitySubsetFeaturesKHR portabilityFeatures{};
            portabilityFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PORTABILITY_SUBSET_FEATURES_KHR;
            portabilityFeatures.pNext = &bufAddrFeatures;
            portabilityFeatures.events = true;
            timelineFeatures.pNext = &portabilityFeatures;
        #else
            timelineFeatures.pNext = &syncFeatures;
        #endif
        
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

        FL_LOG_INFO("%d vulkan device extensions enabled", createInfo.enabledExtensionCount);
        for (u32 i = 0; i < createInfo.enabledExtensionCount; i++)
            FL_LOG_INFO("    %s", createInfo.ppEnabledExtensionNames[i]);

        FL_VK_ENSURE_RESULT(vkCreateDevice(m_PhysicalDevice, &createInfo, nullptr, &m_Device), "Create vulkan device");

        // Load all device functions for this device. This will have to change
        // if we ever support multiple devices.
        volkLoadDevice(m_Device);
    }

    void Devices::Shutdown()
    {
        FL_LOG_TRACE("Vulkan device shutdown begin");

        vkDestroyDevice(m_Device, nullptr);
        m_Device = nullptr;
        m_PhysicalDevice = nullptr;
    }

    bool Devices::CheckDeviceCompatability(VkPhysicalDevice device, const std::vector<const char*>& extensions)
    {
        QueueFamilyIndices indices = Queues::GetQueueFamilies(device);
        
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(device, &props);
        
        FL_LOG_DEBUG("Checking compatability of graphics device");
        DumpDeviceInfo(LogLevel::Debug, props);
        
        if (props.apiVersion < Context::VulkanApiVersion)
        {
            FL_LOG_DEBUG("Compatible - no. Vulkan version too low");
            return false;
        }

        // Get hardware extension support
        u32 supportedExtensionCount = 0;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &supportedExtensionCount, nullptr);
        std::vector<VkExtensionProperties> supportedExtensions(supportedExtensionCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &supportedExtensionCount, supportedExtensions.data());

        // Ensure extension compatability
        for (auto extension : extensions)
        {
            if (!Common::SupportsExtension(supportedExtensions, extension))
            {
                FL_LOG_DEBUG("Compatible - no. Missing required extension %s", extension);
                return false;
            }
        }
        
        if (!indices.IsComplete())
        {
            FL_LOG_DEBUG("Compatible - no. Missing full queue family support");
            return false;
        }
            
        return true;
    }

    void Devices::PopulateOptionalExtensions(std::vector<const char*>& extensions, const ContextInitializeInfo& initInfo)
    {
        // Ensure portability subset is enabled on mac but only if it is supported
        #ifdef FL_PLATFORM_MACOS
            if (Common::SupportsExtension(m_SupportedExtensions, "VK_KHR_portability_subset"))
                extensions.push_back("VK_KHR_portability_subset");
        #endif
        
        #ifdef FL_DEBUG
        if (Common::SupportsExtension(m_SupportedExtensions, "VK_NV_device_diagnostic_checkpoints"))
            extensions.push_back("VK_NV_device_diagnostic_checkpoints");
        #endif
    }

    void Devices::PopulateFeatureTable(VkPhysicalDeviceFeatures& features, std::vector<const char*>& extensions, const ContextInitializeInfo& initInfo)
    {
        VkPhysicalDeviceFeatures supported;
        vkGetPhysicalDeviceFeatures(m_PhysicalDevice, &supported);
        
        if (initInfo.RequestedFeatures.SamplerAnisotropy)
        {
            if (supported.samplerAnisotropy)
            {
                features.samplerAnisotropy = true;
                Flourish::Context::FeatureTable().SamplerAnisotropy = true;
            }
            else
            { FL_LOG_WARN("SamplerAnisotropy was requested but not supported"); }
        }

        if (initInfo.RequestedFeatures.IndependentBlend)
        {
            if (supported.independentBlend)
            {
                features.independentBlend = true;
                Flourish::Context::FeatureTable().IndependentBlend = true;
            }
            else
            { FL_LOG_WARN("IndependentBlend was requested but not supported"); }
        }

        if (initInfo.RequestedFeatures.WideLines)
        {
            if (supported.wideLines)
            {
                features.wideLines = true;
                Flourish::Context::FeatureTable().WideLines = true;
            }
            else
            { FL_LOG_WARN("WideLines was requested but not supported"); }
        }
        
        if (initInfo.RequestedFeatures.SamplerMinMax)
        {
            if (Common::SupportsExtension(m_SupportedExtensions, VK_EXT_SAMPLER_FILTER_MINMAX_EXTENSION_NAME))
            {
                extensions.push_back(VK_EXT_SAMPLER_FILTER_MINMAX_EXTENSION_NAME);
                Flourish::Context::FeatureTable().SamplerMinMax = true;
            }
            else
            { FL_LOG_WARN("SamplerMinMax was requested but not supported"); }
        }

        if (initInfo.RequestedFeatures.RayTracing)
        {
            if (supported.shaderInt64 &&
                Common::SupportsExtension(m_SupportedExtensions, VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME) &&
                Common::SupportsExtension(m_SupportedExtensions, VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME) &&
                Common::SupportsExtension(m_SupportedExtensions, VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME))
            {
                features.shaderInt64 = true;
                extensions.push_back(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
                extensions.push_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
                extensions.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
                Flourish::Context::FeatureTable().RayTracing = true;
            }
            else
            { FL_LOG_WARN("RayTracing was requested but not supported"); }
        }
    }

    void Devices::PopulateDeviceProperties()
    {
        if (Flourish::Context::FeatureTable().RayTracing)
        {
            m_RayTracingProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
            m_AccelStructureProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR;

            m_AccelStructureProperties.pNext = &m_RayTracingProperties;

            VkPhysicalDeviceProperties2 devProps{};
            devProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
            devProps.pNext = &m_AccelStructureProperties;

            vkGetPhysicalDeviceProperties2(m_PhysicalDevice, &devProps);
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

    void Devices::DumpDeviceInfo(LogLevel logLevel, const VkPhysicalDeviceProperties& props)
    {
        const char* deviceTypeName = "Other";
        switch (props.deviceType)
        {
            default: break;
            case VK_PHYSICAL_DEVICE_TYPE_CPU:
            { deviceTypeName = "CPU"; } break;
            case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
            { deviceTypeName = "Discrete GPU"; } break;
            case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
            { deviceTypeName = "Integrated GPU"; } break;
            case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
            { deviceTypeName = "Virtual GPU"; } break;
        }

        FL_LOG(logLevel, "Device (ID %d)", props.deviceID);       
        FL_LOG(logLevel, "    Name: %s", props.deviceName);
        FL_LOG(logLevel, "    Type: %s", deviceTypeName);
        FL_LOG(logLevel, "    Vendor: %d", props.vendorID);
        FL_LOG(logLevel, "    Driver Ver: %d", props.driverVersion);
        FL_LOG(
            logLevel,
            "    Vulkan Ver: %d.%d.%d",
            VK_API_VERSION_MAJOR(props.apiVersion),
            VK_API_VERSION_MINOR(props.apiVersion),
            VK_API_VERSION_PATCH(props.apiVersion)
        );
    }
}
