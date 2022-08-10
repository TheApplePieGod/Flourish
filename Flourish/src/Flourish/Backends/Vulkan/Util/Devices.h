#pragma once

#include "Flourish/Backends/Vulkan/Util/Common.h"

namespace Flourish::Vulkan
{
    class Devices
    {
    public:
        void Initialize(const ContextInitializeInfo& initInfo);
        void Shutdown();

        // TS
        inline VkPhysicalDevice PhysicalDevice() const { return m_PhysicalDevice; }
        inline VkDevice Device() const { return m_Device; }
        inline VkSampleCountFlagBits MaxMsaaSamples() const { return m_DeviceMaxSampleCount; }

    private:
        bool CheckDeviceCompatability(VkPhysicalDevice device, const std::vector<const char*>& extensions);
        VkSampleCountFlagBits GetMaxSampleCount();

    private:
        VkPhysicalDevice m_PhysicalDevice;
        VkSampleCountFlagBits m_DeviceMaxSampleCount;
        VkDevice m_Device;
    };
}