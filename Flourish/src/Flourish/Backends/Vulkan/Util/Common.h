#pragma once

#include "Flourish/Api/Context.h"
#include "Flourish/Api/RenderPass.h"
#include "Flourish/Api/Texture.h"
#include "Flourish/Api/AccelerationStructure.h"
#include "volk/volk.h"
#include "vk_mem_alloc.h"

namespace Flourish::Vulkan
{
    struct Common
    {
        static bool SupportsExtension(const std::vector<VkExtensionProperties>& extensions, const char* extension);
        static VkFormat ConvertColorFormat(ColorFormat format);
        static ColorFormat RevertColorFormat(VkFormat format);
        static VkAttachmentLoadOp ConvertAttachmentInitialization(AttachmentInitialization init);
        static VkSampleCountFlagBits ConvertMsaaSampleCount(MsaaSampleCount sampleCount);
        static VkPrimitiveTopology ConvertVertexTopology(VertexTopology topology);
        static VkFormat ConvertBufferDataType(BufferDataType type);
        static VkCullModeFlagBits ConvertCullMode(CullMode mode);
        static VkFrontFace ConvertWindingOrder(WindingOrder order);
        static VkCompareOp ConvertDepthComparison(DepthComparison comp);
        static VkDescriptorType ConvertShaderResourceType(ShaderResourceType type);
        static ShaderResourceType RevertShaderResourceType(VkDescriptorType type);
        static VkShaderStageFlags ConvertShaderAccessType(ShaderType type);
        static VkBlendFactor ConvertBlendFactor(BlendFactor factor);
        static VkBlendOp ConvertBlendOperation(BlendOperation op);
        static VkFilter ConvertSamplerFilter(SamplerFilter filter);
        static VkSamplerAddressMode ConvertSamplerWrapMode(SamplerWrapMode mode);
        static VkSamplerReductionMode ConvertSamplerReductionMode(SamplerReductionMode mode);
        static VkAccelerationStructureTypeKHR ConvertAccelerationStructureType(AccelerationStructureType type);

        static bool CheckResult(VkResult result, bool ensure, const char* name);
    };
}

#define FL_VK_CHECK_RESULT(func, name) ::Flourish::Vulkan::Common::CheckResult(func, false, name)
#define FL_VK_ENSURE_RESULT(func, name) { auto result = func; ::Flourish::Vulkan::Common::CheckResult(result, true, name); }
