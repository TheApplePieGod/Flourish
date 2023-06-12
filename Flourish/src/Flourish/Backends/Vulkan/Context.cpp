#include "flpch.h"
#include "Context.h"

#include "Flourish/Backends/Vulkan/RenderContext.h"

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

        FL_LOG_DEBUG("Vulkan context ready");
    }

    void Context::Shutdown(std::function<void()> finalizer)
    {
        FL_LOG_TRACE("Vulkan context shutdown begin");

        Sync();

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
        s_SubmissionHandler.WaitOnFrameSemaphores();
    }

    void Context::EndFrame()
    {
        s_FinalizerQueue.Iterate();
        //s_SubmissionHandler.ProcessFrameSubmissions({}, true);
        s_SubmissionHandler.ProcessFrameSubmissions2(nullptr, 0, true);
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
            #elif defined(FL_PLATFORM_LINUX)
                "VK_KHR_xcb_surface",
            #elif defined (FL_PLATFORM_MACOS)
                "VK_MVK_macos_surface",
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
        #else
            createInfo.enabledLayerCount = 0;
        #endif
        createInfo.enabledExtensionCount = static_cast<u32>(requiredExtensions.size());
        createInfo.ppEnabledExtensionNames = requiredExtensions.data();
        #ifdef FL_PLATFORM_MACOS
            if (Common::SupportsExtension(supportedExtensions, "VK_KHR_portability_enumeration"))
                createInfo.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
        #endif

        FL_LOG_INFO("%d vulkan instance extensions enabled", createInfo.enabledExtensionCount);
        for (u32 i = 0; i < createInfo.enabledExtensionCount; i++)
            FL_LOG_INFO("    %s", createInfo.ppEnabledExtensionNames[i]);
        
        FL_VK_ENSURE_RESULT(vkCreateInstance(&createInfo, nullptr, &s_Instance), "Vulkan create instance");

        // Load all instance functions
        volkLoadInstance(s_Instance);

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
