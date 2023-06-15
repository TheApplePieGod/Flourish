#pragma once

#include "Flourish/Backends/Vulkan/Util/Common.h"
#include "Flourish/Backends/Vulkan/Util/Devices.h"
#include "Flourish/Backends/Vulkan/Util/Queues.h"
#include "Flourish/Backends/Vulkan/Util/Commands.h"
#include "Flourish/Backends/Vulkan/Util/FinalizerQueue.h"
#include "Flourish/Backends/Vulkan/Util/SubmissionHandler.h"

namespace Flourish::Vulkan
{
    class RenderContext;
    class Context
    {
    public:
        // TS
        inline static void Sync() { vkDeviceWaitIdle(s_Devices.Device()); }

        // TS
        inline static VkInstance Instance() { return s_Instance; }
        inline static const Devices& Devices() { return s_Devices; }
        inline static Queues& Queues() { return s_Queues; }
        inline static Commands& Commands() { return s_Commands; }
        inline static FinalizerQueue& FinalizerQueue() { return s_FinalizerQueue; }
        inline static SubmissionHandler& SubmissionHandler() { return s_SubmissionHandler; }
        inline static VmaAllocator Allocator() { return s_Allocator; }
        inline static const auto& ValidationLayers() { return s_ValidationLayers; }

        // Need to stay on 1.2 for now to support MoltenVK
        inline static constexpr u32 VulkanApiVersion = VK_API_VERSION_1_2;

    private:
        static void Initialize(const ContextInitializeInfo& initInfo);
        static void Shutdown(std::function<void()> finalizer = nullptr);
        static void BeginFrame();
        static void EndFrame();
        static void SetupInstance(const ContextInitializeInfo& initInfo);
        static void SetupAllocator();
        static void ConfigureValidationLayers();

    private:
        inline static VkInstance s_Instance = nullptr;
        inline static Vulkan::Devices s_Devices;
        inline static Vulkan::Queues s_Queues;
        inline static Vulkan::Commands s_Commands;
        inline static Vulkan::FinalizerQueue s_FinalizerQueue;
        inline static Vulkan::SubmissionHandler s_SubmissionHandler;
        inline static VmaAllocator s_Allocator;
        inline static VkDebugUtilsMessengerEXT s_DebugMessenger = nullptr;
        inline static std::vector<const char*> s_ValidationLayers;

        friend class Flourish::Context;
    };
}
