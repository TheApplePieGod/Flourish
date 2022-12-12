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
        inline const VkPhysicalDeviceProperties& PhysicalDeviceProperties() const { return m_PhysicalDeviceProperties; }
        inline VkDevice Device() const { return m_Device; }
        inline VkSampleCountFlagBits MaxMsaaSamples() const { return m_DeviceMaxSampleCount; }

    private:
        bool CheckDeviceCompatability(VkPhysicalDevice device, const std::vector<const char*>& extensions);
        void PopulateOptionalExtensions(std::vector<const char*>& extensions, const ContextInitializeInfo& initInfo);
        void PopulateDeviceFeatures(VkPhysicalDeviceFeatures& features, const ContextInitializeInfo& initInfo);
        VkSampleCountFlagBits GetMaxSampleCount();

    private:
        VkPhysicalDevice m_PhysicalDevice;
        VkPhysicalDeviceProperties m_PhysicalDeviceProperties;
        VkSampleCountFlagBits m_DeviceMaxSampleCount;
        VkDevice m_Device;
    };
}