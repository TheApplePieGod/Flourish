#pragma once

#include "Flourish/Backends/Vulkan/Util/Common.h"
#include "Flourish/Backends/Vulkan/Util/Devices.h"
#include "Flourish/Backends/Vulkan/Util/Queues.h"
#include "Flourish/Backends/Vulkan/Util/Commands.h"
#include "Flourish/Backends/Vulkan/Util/DeleteQueue.h"
#include "Flourish/Api/Context.h"

namespace Flourish::Vulkan
{
    class Context
    {
    public:
        // TS
        inline static void Sync() { vkDeviceWaitIdle(s_Devices.Device()); }

        // TS
        inline static VkInstance Instance() { return s_Instance; }
        inline static const Devices& Devices() { return s_Devices; }
        inline static const Queues& Queues() { return s_Queues; }
        inline static const Commands& Commands() { return s_Commands; }
        inline static DeleteQueue& DeleteQueue() { return s_DeleteQueue; }
        inline static const auto& ValidationLayers() { return s_ValidationLayers; }
        inline static u32 FrameIndex() { return s_FrameIndex; }

    private:
        static void Initialize(const ContextInitializeInfo& initInfo);
        static void Shutdown();
        static void BeginFrame();
        static void EndFrame();
        static void SetupInstance(const ContextInitializeInfo& initInfo);
        static void ConfigureValidationLayers();

    private:
        inline static VkInstance s_Instance = nullptr;
        inline static Vulkan::Devices s_Devices;
        inline static Vulkan::Queues s_Queues;
        inline static Vulkan::Commands s_Commands;
        inline static Vulkan::DeleteQueue s_DeleteQueue;
        inline static VkDebugUtilsMessengerEXT s_DebugMessenger = nullptr;
        inline static std::vector<const char*> s_ValidationLayers;
        inline static u32 s_FrameIndex = 0;
        
        friend class Flourish::Context;
    };
}