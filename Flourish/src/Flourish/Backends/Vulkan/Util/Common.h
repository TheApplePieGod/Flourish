#pragma once

#include "Flourish/Api/Context.h"
#include "Flourish/Api/RenderPass.h"
#include "Flourish/Api/Pipeline.h"
#include "Flourish/Api/Texture.h"
#include "vulkan/vulkan.h"
#include "vk_mem_alloc.h"
#ifdef FL_PLATFORM_WINDOWS
    #include "vulkan/vulkan_win32.h"
#elif defined(FL_PLATFORM_LINUX)
    #include "vulkan/vulkan_xcb.h"
#endif

namespace Flourish::Vulkan
{
    struct Common
    {
        static bool SupportsExtension(const std::vector<VkExtensionProperties>& extensions, const char* extension);
        static VkFormat ConvertColorFormat(ColorFormat format);
        static VkSampleCountFlagBits ConvertMsaaSampleCount(MsaaSampleCount sampleCount);
        static VkPrimitiveTopology ConvertVertexTopology(VertexTopology topology);
        static VkFormat ConvertBufferDataType(BufferDataType type);
        static VkCullModeFlagBits ConvertCullMode(CullMode mode);
        static VkFrontFace ConvertWindingOrder(WindingOrder order);
        static VkDescriptorType ConvertShaderResourceType(ShaderResourceType type);
        static VkShaderStageFlags ConvertShaderResourceAccessType(ShaderResourceAccessType type);
        static VkBlendFactor ConvertBlendFactor(BlendFactor factor);
        static VkBlendOp ConvertBlendOperation(BlendOperation op);
        static VkFilter ConvertSamplerFilter(SamplerFilter filter);
        static VkSamplerAddressMode ConvertSamplerWrapMode(SamplerWrapMode mode);
        static VkSamplerReductionMode ConvertSamplerReductionMode(SamplerReductionMode mode);
    };
}

#ifdef FL_DEBUG
    #define FL_VK_CHECK_RESULT(func) { auto result = func; FL_ASSERT(result == VK_SUCCESS, "Vulkan function failed with error %d", result); }
#else
    #define FL_VK_CHECK_RESULT(func)
#endif

#define FL_VK_ENSURE_RESULT(func) { auto result = func; FL_CRASH_ASSERT(result == VK_SUCCESS, "Critical vulkan function failed with error %d", result); }