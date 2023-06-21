#pragma once

#include "Flourish/Core/Log.h"
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
        inline const auto& PhysicalDeviceProperties() const { return m_PhysicalDeviceProperties; }
        inline const auto& RayTracingProperties() const { return m_RayTracingProperties; }
        inline const auto& AccelStructureProperties() const { return m_AccelStructureProperties; }

    private:
        bool CheckDeviceCompatability(VkPhysicalDevice device, const std::vector<const char*>& extensions);
        void PopulateOptionalExtensions(std::vector<const char*>& extensions, const ContextInitializeInfo& initInfo);
        void PopulateFeatureTable(VkPhysicalDeviceFeatures& features, std::vector<const char*>& extensions, const ContextInitializeInfo& initInfo);
        void PopulateDeviceProperties();
        VkSampleCountFlagBits GetMaxSampleCount();
        void DumpDeviceInfo(LogLevel logLevel, const VkPhysicalDeviceProperties& props);

    private:
        std::vector<VkExtensionProperties> m_SupportedExtensions;
        VkPhysicalDevice m_PhysicalDevice;
        VkPhysicalDeviceProperties m_PhysicalDeviceProperties;
        VkPhysicalDeviceRayTracingPipelinePropertiesKHR m_RayTracingProperties;
        VkPhysicalDeviceAccelerationStructurePropertiesKHR m_AccelStructureProperties;
        VkSampleCountFlagBits m_DeviceMaxSampleCount;
        VkDevice m_Device;
    };
}
