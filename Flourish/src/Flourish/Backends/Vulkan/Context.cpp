#include "flpch.h"
#include "Context.h"

#include "Flourish/Backends/Vulkan/RenderContext.h"
#include "Flourish/Backends/Vulkan/Util/DescriptorBinder.h"

#ifdef FL_PLATFORM_MACOS
    #include "MoltenVK/mvk_vulkan.h"
#endif

namespace Flourish::Vulkan
{
    static VKAPI_ATTR VkBool32 VKAPI_CALL VulkanDebugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData)
    {
        switch (messageSeverity)
        {
            default:
                break;
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT: { FL_LOG_TRACE("%s", pCallbackData->pMessage); } return VK_TRUE;
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT: { FL_LOG_INFO("%s", pCallbackData->pMessage); } return VK_TRUE;
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT: { FL_LOG_WARN("%s", pCallbackData->pMessage); } return VK_TRUE;
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT: { FL_LOG_ERROR("%s", pCallbackData->pMessage); } return VK_TRUE;
        }

        return VK_FALSE;
    }

    void Context::Initialize(const ContextInitializeInfo& initInfo)
    {
        FL_LOG_DEBUG("Initializing vulkan context");
        
        // Initialize the vulkan loader
        FL_VK_ENSURE_RESULT(volkInitialize(), "Vulkan loader initialization");

        SetupInstance(initInfo);
        s_Devices.Initialize(initInfo);
        SetupAllocator();
        s_Queues.Initialize();
        s_Commands.Initialize();
        s_SubmissionHandler.Initialize();
        s_FinalizerQueue.Initialize();

        // Create global empty descriptor set layout
        PipelineDescriptorData::Initialize();

        FL_LOG_DEBUG("Vulkan context ready");
    }

    void Context::Shutdown(std::function<void()> finalizer)
    {
        FL_LOG_TRACE("Vulkan context shutdown begin");

        Sync();

        PipelineDescriptorData::Shutdown();

        FL_LOG_TRACE("Running vulkan finalizer pass #1");
        s_FinalizerQueue.Shutdown();
        if (finalizer)
            finalizer();
        FL_LOG_TRACE("Running vulkan finalizer pass #2");
        s_FinalizerQueue.Shutdown();
        s_Queues.Shutdown();
        s_SubmissionHandler.Shutdown();
        s_Commands.Shutdown();
        vmaDestroyAllocator(s_Allocator);
        s_Devices.Shutdown();
        #if FL_DEBUG
            // Find func and destroy debug instance
            // TODO: stop doing a load here?
            auto destroyDebugUtilsFunc = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(s_Instance, "vkDestroyDebugUtilsMessengerEXT");
            if (destroyDebugUtilsFunc)
                destroyDebugUtilsFunc(s_Instance, s_DebugMessenger, nullptr);
        #endif
        FL_LOG_TRACE("Destroying vulkan instance");
        vkDestroyInstance(s_Instance, nullptr);

        FL_LOG_DEBUG("Vulkan context shutdown complete");
    }

    void Context::BeginFrame()
    {
        vmaSetCurrentFrameIndex(s_Allocator, (u32)Flourish::Context::FrameCount());
        s_SubmissionHandler.WaitOnFrameSemaphores();
    }

    void Context::EndFrame()
    {
        s_FinalizerQueue.Iterate();
        s_SubmissionHandler.ProcessFrameSubmissions();
    }

    MemoryStatistics Context::ComputeMemoryStatistics()
    {
        MemoryStatistics stats{};

        VmaBudget budgets[VK_MAX_MEMORY_HEAPS] = {};
        vmaGetHeapBudgets(s_Allocator, budgets);

        const VkPhysicalDeviceMemoryProperties* memProps;
        vmaGetMemoryProperties(s_Allocator, &memProps);
        
        for (u32 i = 0; i < memProps->memoryHeapCount; i++)
        {
            const VmaBudget& budget = budgets[i];

            stats.AllocationCount += budget.statistics.allocationCount;
            stats.AllocationTotalSize += budget.statistics.allocationBytes;

            stats.BlockCount += budget.statistics.blockCount;
            stats.BlockTotalSize += budget.statistics.blockBytes;

            stats.TotalAvailable += budget.budget;
        }

        return stats;
    }

    void Context::SetupInstance(const ContextInitializeInfo& initInfo)
    {
        FL_LOG_TRACE("Vulkan instance setup begin");

        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = initInfo.ApplicationName;
        appInfo.applicationVersion = VK_MAKE_API_VERSION(0,
            initInfo.MajorVersion,
            initInfo.MinorVersion,
            initInfo.PatchVersion
        );
        appInfo.pEngineName = "Flourish";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VulkanApiVersion;

        // Get api extension support
        u32 supportedExtensionCount = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &supportedExtensionCount, nullptr);
        std::vector<VkExtensionProperties> supportedExtensions(supportedExtensionCount);
        vkEnumerateInstanceExtensionProperties(nullptr, &supportedExtensionCount, supportedExtensions.data());

        // Required extensions
        std::vector<const char*> requiredExtensions = {
            VK_KHR_SURFACE_EXTENSION_NAME,
            #ifdef FL_PLATFORM_WINDOWS
                "VK_KHR_win32_surface",
            #elif defined(FL_PLATFORM_ANDROID)
                "VK_KHR_android_surface"
            #elif defined(FL_PLATFORM_LINUX)
                "VK_KHR_xcb_surface",
            #elif defined (FL_PLATFORM_MACOS)
                "VK_EXT_metal_surface"
            #endif
        };

        // Ensure compatability
        for (auto extension : requiredExtensions)
        {
            bool found = Common::SupportsExtension(supportedExtensions, extension);
            FL_CRASH_ASSERT(found, "Vulkan driver is missing the required extension '%s'", extension);
        }
        
        // Ensure portability enumeration is enabled on mac but only if it is supported
        #ifdef FL_PLATFORM_MACOS
            if (Common::SupportsExtension(supportedExtensions, "VK_KHR_portability_enumeration"))
                requiredExtensions.push_back("VK_KHR_portability_enumeration");
        #endif

        // Create instance
        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;
        #if FL_DEBUG
            FL_LOG_TRACE("Configuring validation layers");

            const char* debugExt = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
            bool supportsDebug = Common::SupportsExtension(supportedExtensions, debugExt);
            if (supportsDebug)
                requiredExtensions.push_back(debugExt);

            ConfigureValidationLayers();

            createInfo.enabledLayerCount = static_cast<u32>(s_ValidationLayers.size());
            createInfo.ppEnabledLayerNames = s_ValidationLayers.data();

            FL_LOG_INFO("%d vulkan validation layers enabled", createInfo.enabledLayerCount);
            for (u32 i = 0; i < createInfo.enabledLayerCount; i++)
                FL_LOG_INFO("    %s", createInfo.ppEnabledLayerNames[i]);

            VkValidationFeatureEnableEXT enables[] = {
                VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT,
                //VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT,
                //VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT
            };
            VkValidationFeaturesEXT features = {};
            features.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
            features.enabledValidationFeatureCount = 3;
            features.pEnabledValidationFeatures = enables;
            if (Common::SupportsExtension(supportedExtensions, "VK_EXT_validation_features"))
            {
                requiredExtensions.push_back("VK_EXT_validation_features");
                Common::IterateAndWriteNextChain(&createInfo.pNext, &features);
            }
        #else
            createInfo.enabledLayerCount = 0;
        #endif

        const char* layerExt = VK_EXT_LAYER_SETTINGS_EXTENSION_NAME;
        bool supportsLayerSettings = Common::SupportsExtension(supportedExtensions, layerExt);
        if (supportsLayerSettings)
            requiredExtensions.push_back(layerExt);

        createInfo.enabledExtensionCount = static_cast<u32>(requiredExtensions.size());
        createInfo.ppEnabledExtensionNames = requiredExtensions.data();

        std::vector<VkLayerSettingEXT> layerSettings;
        #ifdef FL_PLATFORM_MACOS
            if (Common::SupportsExtension(supportedExtensions, "VK_KHR_portability_enumeration"))
                createInfo.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;

            // Enable metal argument buffers. For now, this is mandatory because we haven't provided a way
            // for the user to know that the limits of descriptor indexing are. This is fine
            // for now since argument buffers are supported in osx >= 11.0
            // https://github.com/KhronosGroup/MoltenVK/issues/1610
            VkLayerSettingEXT& settings = layerSettings.emplace_back();
            u32 useMetalArgumentBuffers = 1;
            settings.type = VK_LAYER_SETTING_TYPE_UINT32_EXT;
            settings.pLayerName = kMVKMoltenVKDriverLayerName;
            settings.pSettingName = "MVK_CONFIG_USE_METAL_ARGUMENT_BUFFERS";
            settings.pValues = &useMetalArgumentBuffers;
            settings.valueCount = 1;
        #endif

        VkLayerSettingsCreateInfoEXT layerSettingsInfo{};
        layerSettingsInfo.sType = VK_STRUCTURE_TYPE_LAYER_SETTINGS_CREATE_INFO_EXT;
        layerSettingsInfo.pSettings = layerSettings.data();
        layerSettingsInfo.settingCount = layerSettings.size();
        if (supportsLayerSettings)
            Common::IterateAndWriteNextChain(&createInfo.pNext, &layerSettingsInfo);

        FL_LOG_INFO("%d vulkan instance extensions enabled", createInfo.enabledExtensionCount);
        for (u32 i = 0; i < createInfo.enabledExtensionCount; i++)
            FL_LOG_INFO("    %s", createInfo.ppEnabledExtensionNames[i]);
        
        FL_VK_ENSURE_RESULT(vkCreateInstance(&createInfo, nullptr, &s_Instance), "Vulkan create instance");

        // Load all instance functions
        volkLoadInstanceOnly(s_Instance);

        #if FL_DEBUG
            // Setup debug messenger
            if (supportsDebug)
            {
                FL_LOG_DEBUG("Configuring debug utilities");

                VkDebugUtilsMessengerCreateInfoEXT createInfo{};
                createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
                createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
                createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
                #if FL_LOGGING
                    createInfo.pfnUserCallback = VulkanDebugCallback;
                #endif

                // Locate extension creation function and run
                auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(s_Instance, "vkCreateDebugUtilsMessengerEXT");
                bool success = false;
                if (func)
                    success = func(s_Instance, &createInfo, nullptr, &s_DebugMessenger) == VK_SUCCESS;
            
                if (!success)
                    FL_LOG_WARN("Unable to initialize vulkan debug utilities") ;
            }   
        #endif

        FL_LOG_TRACE("Instance setup complete");
    }

    void Context::SetupAllocator()
    {
        VmaVulkanFunctions vulkanFunctions = {};
        vulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
        vulkanFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

        VmaAllocatorCreateInfo createInfo{};
        createInfo.instance = s_Instance;
        createInfo.physicalDevice = s_Devices.PhysicalDevice();
        createInfo.device = s_Devices.Device();
        createInfo.vulkanApiVersion = VulkanApiVersion;
        createInfo.pVulkanFunctions = &vulkanFunctions;
        if (Flourish::Context::FeatureTable().BufferGPUAddress)
            createInfo.flags |= VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
        if (Devices().SupportsMemoryBudget())
            createInfo.flags |= VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;

        FL_VK_ENSURE_RESULT(vmaCreateAllocator(&createInfo, &s_Allocator), "Vulkan create allocator");
    }

    void Context::ConfigureValidationLayers()
    {
        std::array<const char*, 1> requestedLayers = {
            "VK_LAYER_KHRONOS_validation"
        };

        u32 supportedLayerCount;
        vkEnumerateInstanceLayerProperties(&supportedLayerCount, nullptr);
        std::vector<VkLayerProperties> supportedLayers(supportedLayerCount);
        vkEnumerateInstanceLayerProperties(&supportedLayerCount, supportedLayers.data());

        // Check for compatability
        for (auto rl : requestedLayers)
        {
            for (auto sl : supportedLayers)
            {
                if (strcmp(rl, sl.layerName) == 0)
                {
                    s_ValidationLayers.push_back(rl);
                    break;
                }
            }
        }
    }
}
