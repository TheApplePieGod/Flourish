#pragma once

#include "Flourish/Api/Context.h"
#include "Flourish/Api/RenderPass.h"
#include "Flourish/Api/Pipeline.h"
#include "Flourish/Api/Texture.h"
#include "volk/volk.h"
#include "vk_mem_alloc.h"

namespace Flourish::Vulkan
{
    struct Common
    {
        static bool SupportsExtension(const std::vector<VkExtensionProperties>& extensions, const char* extension);
        static VkFormat ConvertColorFormat(ColorFormat format);
        static ColorFormat RevertColorFormat(VkFormat format);
        static VkSampleCountFlagBits ConvertMsaaSampleCount(MsaaSampleCount sampleCount);
        static VkPrimitiveTopology ConvertVertexTopology(VertexTopology topology);
        static VkFormat ConvertBufferDataType(BufferDataType type);
        static VkCullModeFlagBits ConvertCullMode(CullMode mode);
        static VkFrontFace ConvertWindingOrder(WindingOrder order);
        static VkDescriptorType ConvertShaderResourceType(ShaderResourceType type);
        static ShaderResourceType RevertShaderResourceType(VkDescriptorType type);
        static VkShaderStageFlags ConvertShaderResourceAccessType(ShaderResourceAccessType type);
        static VkBlendFactor ConvertBlendFactor(BlendFactor factor);
        static VkBlendOp ConvertBlendOperation(BlendOperation op);
        static VkFilter ConvertSamplerFilter(SamplerFilter filter);
        static VkSamplerAddressMode ConvertSamplerWrapMode(SamplerWrapMode mode);
        static VkSamplerReductionMode ConvertSamplerReductionMode(SamplerReductionMode mode);

        static void CheckResult(VkResult result, bool ensure);
    };
}

#ifdef FL_DEBUG
    #define FL_VK_CHECK_RESULT(func) { auto result = func; ::Flourish::Vulkan::Common::CheckResult(result, false); }
#else
    #define FL_VK_CHECK_RESULT(func) func
#endif

#define FL_VK_ENSURE_RESULT(func) { auto result = func; ::Flourish::Vulkan::Common::CheckResult(result, true); }