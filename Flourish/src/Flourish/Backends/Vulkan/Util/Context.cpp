#include "flpch.h"
#include "Context.h"

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
            //case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT: { FL_LOG_TRACE("%s", pCallbackData->pMessage); } return VK_TRUE;
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT: { FL_LOG_INFO("%s", pCallbackData->pMessage); } return VK_TRUE;
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT: { FL_LOG_WARN("%s", pCallbackData->pMessage); } return VK_TRUE;
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT: { FL_LOG_ERROR("%s", pCallbackData->pMessage); } return VK_TRUE;
        }

        return VK_FALSE;
    }

    void Context::Initialize(const ContextInitializeInfo& initInfo)
    {
        SetupInstance(initInfo);
        s_Devices.Initialize(initInfo);
        SetupAllocator();
        s_Queues.Initialize();
        s_Commands.Initialize();
        s_DeleteQueue.Initialize();

        FL_LOG_DEBUG("Vulkan context ready");
    }

    void Context::Shutdown()
    {
        s_Queues.Shutdown();

        Sync();

        s_DeleteQueue.Shutdown();
        s_Commands.Shutdown();
        vmaDestroyAllocator(s_Allocator);
        s_Devices.Shutdown();
        #if FL_DEBUG
            // Find func and destroy debug instance
            auto destroyDebugUtilsFunc = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(s_Instance, "vkDestroyDebugUtilsMessengerEXT");
            if (destroyDebugUtilsFunc)
                destroyDebugUtilsFunc(s_Instance, s_DebugMessenger, nullptr);
        #endif
        vkDestroyInstance(s_Instance, nullptr);


        FL_LOG_DEBUG("Vulkan context shutdown");
    }

    void Context::BeginFrame()
    {
        s_FrameIndex = (s_FrameIndex + 1) % Flourish::Context::FrameBufferCount();
    }

    void Context::EndFrame()
    {
        s_DeleteQueue.Iterate();
    }

    void Context::SetupInstance(const ContextInitializeInfo& initInfo)
    {
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
            #endif
        };

        // Ensure compatability
        for (auto extension : requiredExtensions)
        {
            bool found = Common::SupportsExtension(supportedExtensions, extension);
            FL_CRASH_ASSERT(found, "Vulkan driver is missing the required extension '%s'", extension);
        }

        // Create instance
        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;
        #if FL_DEBUG
            FL_LOG_DEBUG("Configuring validation layers");
            const char* debugExt = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
            bool supportsDebug = Common::SupportsExtension(supportedExtensions, debugExt);
            if (supportsDebug)
                requiredExtensions.push_back(debugExt);
            ConfigureValidationLayers();
            createInfo.enabledLayerCount = static_cast<u32>(s_ValidationLayers.size());
            createInfo.ppEnabledLayerNames = s_ValidationLayers.data();
        #else
            createInfo.enabledLayerCount = 0;
        #endif
        createInfo.enabledExtensionCount = static_cast<u32>(requiredExtensions.size());
        createInfo.ppEnabledExtensionNames = requiredExtensions.data();

        FL_VK_ENSURE_RESULT(vkCreateInstance(&createInfo, nullptr, &s_Instance));

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
                    FL_LOG_WARN("Unable to initialize debug utilities") ;
            }   
        #endif
    }

    void Context::SetupAllocator()
    {
        VmaAllocatorCreateInfo createInfo{};
        createInfo.instance = s_Instance;
        createInfo.physicalDevice = s_Devices.PhysicalDevice();
        createInfo.device = s_Devices.Device();
        createInfo.vulkanApiVersion = VulkanApiVersion;

        vmaCreateAllocator(&createInfo, &s_Allocator);
    }

    void Context::ConfigureValidationLayers()
    {
        std::array<const char*, 1> requestedLayers = { "VK_LAYER_KHRONOS_validation" };

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

    void Context::RegisterThread()
    {
        s_Commands.CreatePoolsForThread();
    }

    void Context::UnregisterThread()
    {
        s_Commands.DestroyPoolsForThread();
    }
}