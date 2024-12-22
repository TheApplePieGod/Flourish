#include "Flourish/Api/FeatureTable.h"
#include "flpch.h"
#include "Devices.h"

#include "Flourish/Backends/Vulkan/Util/Swapchain.h"
#include "Flourish/Backends/Vulkan/Context.h"

#include "Flourish/Backends/Vulkan/Util/Aftermath.h"

namespace Flourish::Vulkan
{
    void Devices::DeviceFeatures::Populate(const FeatureTable& features, const Devices* device, bool repopulate)
    {
        GeneralFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        TimelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES;
        Sync2Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;
        IndexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
        BufferAddrFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
        AccelFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
        RtQueryFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR;
        RtPipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
        ScalarFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES_EXT;

        void** next = &GeneralFeatures.pNext;

        if (repopulate || device->m_SupportsTimelines)
            next = Common::IterateAndWriteNextChain(next, &TimelineFeatures);
        //next = Common::IterateAndWriteNextChain(next, &Sync2Features);

        if (features.RayTracing || features.BufferGPUAddress)
            next = Common::IterateAndWriteNextChain(next, &BufferAddrFeatures);
        if (features.PartiallyBoundResourceSets)
            next = Common::IterateAndWriteNextChain(next, &IndexingFeatures);
        if (features.RayTracing)
        {
            next = Common::IterateAndWriteNextChain(next, &AccelFeatures);
            next = Common::IterateAndWriteNextChain(next, &RtQueryFeatures);
            next = Common::IterateAndWriteNextChain(next, &RtPipelineFeatures);
            next = Common::IterateAndWriteNextChain(next, &ScalarFeatures);
        }

        if (repopulate)
            vkGetPhysicalDeviceFeatures2(device->PhysicalDevice(), &GeneralFeatures);
    }

    void Devices::Initialize(const ContextInitializeInfo& initInfo)
    {
        FL_LOG_TRACE("Vulkan device initialization begin");

        VkInstance instance = Context::Instance();

        // Required physical device extensions
        std::vector<const char*> deviceExtensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
            VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME // VK 1.2
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

        PopulateFeatureTable(deviceExtensions, initInfo);
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

        // Create device
        VkDeviceCreateInfo createInfo{};

        #if defined(FL_PLATFORM_MACOS)
            VkPhysicalDevicePortabilitySubsetFeaturesKHR portabilityFeatures{};
            portabilityFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PORTABILITY_SUBSET_FEATURES_KHR;
            portabilityFeatures.pNext = m_Features.GeneralFeatures.pNext;
            portabilityFeatures.events = true;
            createInfo.pNext = &portabilityFeatures;
        #elif defined(FL_USE_AFTERMATH)
            Aftermath::Initialize();

            VkDeviceDiagnosticsConfigCreateInfoNV aftermathConfig{};
            aftermathConfig.pNext = m_Features.GeneralFeatures.pNext;
            aftermathConfig.sType = VK_STRUCTURE_TYPE_DEVICE_DIAGNOSTICS_CONFIG_CREATE_INFO_NV;
            aftermathConfig.flags = VK_DEVICE_DIAGNOSTICS_CONFIG_ENABLE_SHADER_DEBUG_INFO_BIT_NV |
                                    VK_DEVICE_DIAGNOSTICS_CONFIG_ENABLE_RESOURCE_TRACKING_BIT_NV |
                                    VK_DEVICE_DIAGNOSTICS_CONFIG_ENABLE_AUTOMATIC_CHECKPOINTS_BIT_NV |
                                    VK_DEVICE_DIAGNOSTICS_CONFIG_ENABLE_SHADER_ERROR_REPORTING_BIT_NV;

            createInfo.pNext = &aftermathConfig;
        #else
            createInfo.pNext = m_Features.GeneralFeatures.pNext;
        #endif
        
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
        createInfo.queueCreateInfoCount = static_cast<u32>(queueCreateInfos.size());
        createInfo.pEnabledFeatures = &m_Features.GeneralFeatures.features;
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

        #ifdef FL_USE_AFTERMATH
            Aftermath::Shutdown();
        #endif
    }

    bool Devices::CheckDeviceCompatability(VkPhysicalDevice device, const std::vector<const char*>& extensions)
    {
        QueueFamilyIndices indices = Queues::GetQueueFamilies(device);
        
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(device, &props);

        FL_LOG_DEBUG("Checking compatability of graphics device");
        
        // Get hardware extension support
        u32 supportedExtensionCount = 0;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &supportedExtensionCount, nullptr);
        std::vector<VkExtensionProperties> supportedExtensions(supportedExtensionCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &supportedExtensionCount, supportedExtensions.data());

        DumpDeviceInfo(LogLevel::Debug, props, supportedExtensions);
        
        if (props.apiVersion < Context::VulkanApiVersion)
        {
            FL_LOG_DEBUG("Compatible - no. Vulkan version too low");
            return false;
        }

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
            FL_LOG_DEBUG("    Graphics: %d", indices.GraphicsFamily.value_or(-1));
            FL_LOG_DEBUG("    Compute:  %d", indices.ComputeFamily.value_or(-1));
            FL_LOG_DEBUG("    Transfer: %d", indices.TransferFamily.value_or(-1));
            FL_LOG_DEBUG("    Present:  %d", indices.PresentFamily.value_or(-1));
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

        #ifdef FL_PLATFORM_WINDOWS
            if (Common::SupportsExtension(m_SupportedExtensions, "VK_EXT_full_screen_exclusive") &&
                Common::SupportsExtension(m_SupportedExtensions, "VK_KHR_get_surface_capabilities2"))
            {
                extensions.push_back("VK_EXT_full_screen_exclusive");
                extensions.push_back("VK_KHR_get_surface_capabilities2");
                m_FullScreenExclusive = true;
            }
        #endif
        
        #ifdef FL_DEBUG
            if (Common::SupportsExtension(m_SupportedExtensions, VK_NV_DEVICE_DIAGNOSTIC_CHECKPOINTS_EXTENSION_NAME))
                extensions.push_back(VK_NV_DEVICE_DIAGNOSTIC_CHECKPOINTS_EXTENSION_NAME);
            if (Common::SupportsExtension(m_SupportedExtensions, VK_NV_DEVICE_DIAGNOSTICS_CONFIG_EXTENSION_NAME))
                extensions.push_back(VK_NV_DEVICE_DIAGNOSTICS_CONFIG_EXTENSION_NAME);
        #endif

        if (Common::SupportsExtension(m_SupportedExtensions, VK_EXT_MEMORY_BUDGET_EXTENSION_NAME))
        {
            extensions.push_back(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME);
            m_SupportsMemoryBudget = true;
        }
    }

    void Devices::PopulateFeatureTable(std::vector<const char*>& extensions, const ContextInitializeInfo& initInfo)
    {
        DeviceFeatures supported;
        supported.Populate(initInfo.RequestedFeatures, this, true);

        // We probably could put this behind a feature flag and check in the DrawInstanced command,
        // but the device support is so high it's probably unnecessary
        FL_CRASH_ASSERT(
            supported.GeneralFeatures.features.drawIndirectFirstInstance,
            "DrawIndirectFirstInstance feature is required to be supported"
        );
        m_Features.GeneralFeatures.features.drawIndirectFirstInstance = true;

        // This is useful and widely supported, so just enable it if available
        m_Features.GeneralFeatures.features.shaderStorageImageReadWithoutFormat = supported.GeneralFeatures.features.shaderStorageImageReadWithoutFormat;
        m_Features.GeneralFeatures.features.shaderStorageImageWriteWithoutFormat = supported.GeneralFeatures.features.shaderStorageImageWriteWithoutFormat;

        if (supported.TimelineFeatures.timelineSemaphore &&
            Common::SupportsExtension(m_SupportedExtensions, VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME))
        {
            // Do not enable this feature on android. Timeline semaphores seem to cause a lot of stability issues,
            // at least from my testing on the Adreno GPU. (It's probably my fault and might be fixed, todo revisit)

            #ifndef FL_PLATFORM_ANDROID
                m_SupportsTimelines = true;
                m_Features.TimelineFeatures.timelineSemaphore = true;
                extensions.push_back(VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME);
            #endif
        }
        
        if (initInfo.RequestedFeatures.SamplerAnisotropy)
        {
            if (supported.GeneralFeatures.features.samplerAnisotropy)
            {
                m_Features.GeneralFeatures.features.samplerAnisotropy = true;
                Flourish::Context::FeatureTable().SamplerAnisotropy = true;
            }
            else
            { FL_LOG_WARN("SamplerAnisotropy was requested but not supported"); }
        }

        if (initInfo.RequestedFeatures.IndependentBlend)
        {
            if (supported.GeneralFeatures.features.independentBlend)
            {
                m_Features.GeneralFeatures.features.independentBlend = true;
                Flourish::Context::FeatureTable().IndependentBlend = true;
            }
            else
            { FL_LOG_WARN("IndependentBlend was requested but not supported"); }
        }

        if (initInfo.RequestedFeatures.WideLines)
        {
            if (supported.GeneralFeatures.features.wideLines)
            {
                m_Features.GeneralFeatures.features.wideLines = true;
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
            if (supported.GeneralFeatures.features.shaderInt64 &&
                supported.BufferAddrFeatures.bufferDeviceAddress &&
                supported.AccelFeatures.accelerationStructure &&
                supported.RtQueryFeatures.rayQuery &&
                supported.RtPipelineFeatures.rayTracingPipeline &&
                supported.ScalarFeatures.scalarBlockLayout &&
                Common::SupportsExtension(m_SupportedExtensions, "VK_EXT_descriptor_indexing") &&
                Common::SupportsExtension(m_SupportedExtensions, VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME) &&
                Common::SupportsExtension(m_SupportedExtensions, VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME) &&
                Common::SupportsExtension(m_SupportedExtensions, VK_KHR_SPIRV_1_4_EXTENSION_NAME) &&
                Common::SupportsExtension(m_SupportedExtensions, VK_EXT_SCALAR_BLOCK_LAYOUT_EXTENSION_NAME) &&
                Common::SupportsExtension(m_SupportedExtensions, VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME) &&
                Common::SupportsExtension(m_SupportedExtensions, VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME) &&
                Common::SupportsExtension(m_SupportedExtensions, VK_KHR_RAY_QUERY_EXTENSION_NAME) &&
                Common::SupportsExtension(m_SupportedExtensions, VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME))
            {
                m_SupportsSpirv14 = true;
                m_Features.GeneralFeatures.features.shaderInt64 = true;
                m_Features.BufferAddrFeatures.bufferDeviceAddress = true;
                m_Features.AccelFeatures.accelerationStructure = true;
                m_Features.RtQueryFeatures.rayQuery = true;
                m_Features.RtPipelineFeatures.rayTracingPipeline = true;
                m_Features.ScalarFeatures.scalarBlockLayout = true;
                extensions.push_back("VK_EXT_descriptor_indexing");
                extensions.push_back(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
                extensions.push_back(VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME);
                extensions.push_back(VK_KHR_SPIRV_1_4_EXTENSION_NAME);
                extensions.push_back(VK_EXT_SCALAR_BLOCK_LAYOUT_EXTENSION_NAME);
                extensions.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
                extensions.push_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
                extensions.push_back(VK_KHR_RAY_QUERY_EXTENSION_NAME);
                extensions.push_back(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
                Flourish::Context::FeatureTable().RayTracing = true;
                Flourish::Context::FeatureTable().BufferGPUAddress = true;
            }
            else
            { FL_LOG_WARN("RayTracing was requested but not supported"); }
        }

        if (initInfo.RequestedFeatures.BufferGPUAddress)
        {
            if (supported.GeneralFeatures.features.shaderInt64 &&
                supported.BufferAddrFeatures.bufferDeviceAddress &&
                Common::SupportsExtension(m_SupportedExtensions, VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME))
            {
                m_Features.GeneralFeatures.features.shaderInt64 = true;
                m_Features.BufferAddrFeatures.bufferDeviceAddress = true;
                extensions.push_back(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
                Flourish::Context::FeatureTable().BufferGPUAddress = true;
            }
            else
            { FL_LOG_WARN("BufferGPUAddress was requested but not supported"); }
        }

        if (initInfo.RequestedFeatures.PartiallyBoundResourceSets)
        {
            if (
                supported.IndexingFeatures.descriptorBindingPartiallyBound &&
                Common::SupportsExtension(m_SupportedExtensions, "VK_EXT_descriptor_indexing"))
            {
                extensions.push_back("VK_EXT_descriptor_indexing");
                m_Features.IndexingFeatures.descriptorBindingPartiallyBound = true;
                Flourish::Context::FeatureTable().PartiallyBoundResourceSets = true;
            }
            else
            { FL_LOG_WARN("PartiallyBoundResourceSets was requested but not supported"); }
        }

        /*
        if (initInfo.RequestedFeatures.BindlessShaderResources)
        {
            if (supported.IndexingFeatures.runtimeDescriptorArray &&
                supported.IndexingFeatures.shaderSampledImageArrayNonUniformIndexing &&
                supported.IndexingFeatures.shaderStorageImageArrayNonUniformIndexing &&
                supported.IndexingFeatures.shaderStorageBufferArrayNonUniformIndexing &&
                supported.IndexingFeatures.shaderStorageBufferArrayNonUniformIndexing)
            {
                m_Features.IndexingFeatures.runtimeDescriptorArray = true;
                m_Features.IndexingFeatures.shaderSampledImageArrayNonUniformIndexing = true;
                m_Features.IndexingFeatures.shaderStorageImageArrayNonUniformIndexing = true;
                m_Features.IndexingFeatures.shaderStorageBufferArrayNonUniformIndexing = true;
                m_Features.IndexingFeatures.shaderStorageBufferArrayNonUniformIndexing = true;
                Flourish::Context::FeatureTable().BindlessShaderResources = true;
            }
            else
            { FL_LOG_WARN("BindlessShaderResources was requested but not supported"); }
        }
        */

        m_Features.Populate(Flourish::Context::FeatureTable(), this, false);
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

    void Devices::DumpDeviceInfo(
        LogLevel logLevel,
        const VkPhysicalDeviceProperties& props,
        const std::vector<VkExtensionProperties>& extensions
    )
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

        FL_LOG(logLevel, "    Supported Extensions:");
        for (auto& ext : extensions)
            FL_LOG(logLevel, "        %s", ext.extensionName);       
    }
}
