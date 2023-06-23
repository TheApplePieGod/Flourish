#include "flpch.h"
#include "Common.h"

#include "Flourish/Backends/Vulkan/Util/DebugCheckpoints.h"

#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#include "vk_mem_alloc.h"

#define VOLK_IMPLEMENTATION
#include "volk/volk.h"

namespace Flourish::Vulkan
{
    bool Common::SupportsExtension(const std::vector<VkExtensionProperties>& extensions, const char* extension)
    {
        return std::find_if(
            extensions.begin(),
            extensions.end(),
            [extension](const VkExtensionProperties& arg)
            {
                return strcmp(arg.extensionName, extension) == 0;
            }
        ) != extensions.end();
    }

    VkFormat Common::ConvertColorFormat(ColorFormat format)
    {
        switch (format)
        {
            default:
            { FL_ASSERT(false, "Vulkan does not support specified ColorFormat"); } break;
            case ColorFormat::RGBA8_UNORM: return VK_FORMAT_R8G8B8A8_UNORM;
            case ColorFormat::RGBA8_SRGB: return VK_FORMAT_R8G8B8A8_SRGB;
            case ColorFormat::BGRA8_UNORM: return VK_FORMAT_B8G8R8A8_UNORM;
            case ColorFormat::RGB8_UNORM: return VK_FORMAT_R8G8B8_UNORM;
            case ColorFormat::BGR8_UNORM: return VK_FORMAT_B8G8R8_UNORM;
            case ColorFormat::R16_FLOAT: return VK_FORMAT_R16_SFLOAT;
            case ColorFormat::RGBA16_FLOAT: return VK_FORMAT_R16G16B16A16_SFLOAT;
            case ColorFormat::R32_FLOAT: return VK_FORMAT_R32_SFLOAT;
            case ColorFormat::RGBA32_FLOAT: return VK_FORMAT_R32G32B32A32_SFLOAT;
            case ColorFormat::Depth: return VK_FORMAT_D32_SFLOAT;
        }

        return VK_FORMAT_UNDEFINED;
    }

    ColorFormat Common::RevertColorFormat(VkFormat format)
    {
        switch (format)
        {
            default:
            { FL_ASSERT(false, "Vulkan does not support specified ColorFormat"); } break;
            case VK_FORMAT_R8G8B8A8_UNORM: return ColorFormat::RGBA8_UNORM;
            case VK_FORMAT_R8G8B8A8_SRGB: return ColorFormat::RGBA8_SRGB;
            case VK_FORMAT_B8G8R8A8_UNORM: return ColorFormat::BGRA8_UNORM;
            case VK_FORMAT_R8G8B8_UNORM: return ColorFormat::RGB8_UNORM;
            case VK_FORMAT_B8G8R8_UNORM: return ColorFormat::BGR8_UNORM;
            case VK_FORMAT_R16_SFLOAT: return ColorFormat::R16_FLOAT;
            case VK_FORMAT_R16G16B16A16_SFLOAT: return ColorFormat::RGBA16_FLOAT;
            case VK_FORMAT_R32_SFLOAT: return ColorFormat::R32_FLOAT;
            case VK_FORMAT_R32G32B32A32_SFLOAT: return ColorFormat::RGBA32_FLOAT;
            case VK_FORMAT_D32_SFLOAT: return ColorFormat::Depth;
        }

        return ColorFormat::None;
    }

    VkAttachmentLoadOp Common::ConvertAttachmentInitialization(AttachmentInitialization init)
    {
        switch (init)
        {
            default:
            { FL_ASSERT(false, "Vulkan does not support specified AttachmentInitialization"); } break;
            case AttachmentInitialization::None: return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            case AttachmentInitialization::Preserve: return VK_ATTACHMENT_LOAD_OP_LOAD;
            case AttachmentInitialization::Clear: return VK_ATTACHMENT_LOAD_OP_CLEAR;
        }

        return VK_ATTACHMENT_LOAD_OP_MAX_ENUM;
    }

    VkSampleCountFlagBits Common::ConvertMsaaSampleCount(MsaaSampleCount sampleCount)
    {
        switch (sampleCount)
        {
            default:
            { FL_ASSERT(false, "Vulkan does not support specified MsaaSampleCount"); } break;
            case MsaaSampleCount::None: return VK_SAMPLE_COUNT_1_BIT;
            case MsaaSampleCount::Two: return VK_SAMPLE_COUNT_2_BIT;
            case MsaaSampleCount::Four: return VK_SAMPLE_COUNT_4_BIT;
            case MsaaSampleCount::Eight: return VK_SAMPLE_COUNT_8_BIT;
            case MsaaSampleCount::Sixteen: return VK_SAMPLE_COUNT_16_BIT;
            case MsaaSampleCount::Thirtytwo: return VK_SAMPLE_COUNT_32_BIT;
            case MsaaSampleCount::Sixtyfour: return VK_SAMPLE_COUNT_64_BIT;
        }

        return VK_SAMPLE_COUNT_1_BIT;
    }

    VkPrimitiveTopology Common::ConvertVertexTopology(VertexTopology topology)
    {
        switch (topology)
        {
            default:
            { FL_ASSERT(false, "Vulkan does not support specified VertexTopology"); } break;
            case VertexTopology::TriangleList: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            case VertexTopology::TriangleStrip: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
            case VertexTopology::TriangleFan: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
            case VertexTopology::PointList: return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
            case VertexTopology::LineList: return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
            case VertexTopology::LineStrip: return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
        }

        return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    }

    VkFormat Common::ConvertBufferDataType(BufferDataType type)
    {
        switch (type)
        {
            default:
            { FL_ASSERT(false, "Vulkan does not support specified BufferDataType"); } break;
            case BufferDataType::Bool: return VK_FORMAT_R32_UINT;
            case BufferDataType::UInt: return VK_FORMAT_R32_UINT;
            case BufferDataType::Double: return VK_FORMAT_R64_SFLOAT;
            case BufferDataType::Int: return VK_FORMAT_R32_SINT;
            case BufferDataType::Int2: return VK_FORMAT_R32G32_SINT;
            case BufferDataType::Int3: return VK_FORMAT_R32G32B32_SINT;
            case BufferDataType::Int4: return VK_FORMAT_R32G32B32A32_SINT;
            case BufferDataType::Float: return VK_FORMAT_R32_SFLOAT;
            case BufferDataType::Float2: return VK_FORMAT_R32G32_SFLOAT;
            case BufferDataType::Float3: return VK_FORMAT_R32G32B32_SFLOAT;
            case BufferDataType::Float4: return VK_FORMAT_R32G32B32A32_SFLOAT;
            //case BufferDataType::Mat3: return VK_FORMAT_UNDEFINED;
            //case BufferDataType::Mat4: return VK_FORMAT_UNDEFINED;
        }

        return VK_FORMAT_UNDEFINED;
    }

    VkCullModeFlagBits Common::ConvertCullMode(CullMode mode)
    {
        switch (mode)
        {
            default:
            { FL_ASSERT(false, "Vulkan does not support specified CullMode"); } break;
            case CullMode::None: return VK_CULL_MODE_NONE;
            case CullMode::Backface: return VK_CULL_MODE_BACK_BIT;
            case CullMode::Frontface: return VK_CULL_MODE_FRONT_BIT;
            case CullMode::BackAndFront: return VK_CULL_MODE_FRONT_AND_BACK;
        }

        return VK_CULL_MODE_NONE;
    }

    VkFrontFace Common::ConvertWindingOrder(WindingOrder order)
    {
        switch (order)
        {
            default:
            { FL_ASSERT(false, "Vulkan does not support specified WindingOrder"); } break;
            case WindingOrder::Clockwise: return VK_FRONT_FACE_CLOCKWISE;
            case WindingOrder::CounterClockwise: return VK_FRONT_FACE_COUNTER_CLOCKWISE;
        }

        return VK_FRONT_FACE_MAX_ENUM;
    }

    VkCompareOp Common::ConvertDepthComparison(DepthComparison comp)
    {
        switch (comp)
        {
            default:
            { FL_ASSERT(false, "Vulkan does not support specified DepthComparison"); } break;
            case DepthComparison::Equal: return VK_COMPARE_OP_EQUAL;
            case DepthComparison::NotEqual: return VK_COMPARE_OP_NOT_EQUAL;
            case DepthComparison::Less: return VK_COMPARE_OP_LESS;
            case DepthComparison::LessOrEqual: return VK_COMPARE_OP_LESS_OR_EQUAL;
            case DepthComparison::Greater: return VK_COMPARE_OP_GREATER;
            case DepthComparison::GreaterOrEqual: return VK_COMPARE_OP_GREATER_OR_EQUAL;
            case DepthComparison::AlwaysTrue: return VK_COMPARE_OP_ALWAYS;
            case DepthComparison::AlwaysFalse: return VK_COMPARE_OP_NEVER;
            case DepthComparison::Auto: return Flourish::Context::ReversedZBuffer() ? VK_COMPARE_OP_GREATER_OR_EQUAL : VK_COMPARE_OP_LESS; }

        return VK_COMPARE_OP_MAX_ENUM;
    }

    VkDescriptorType Common::ConvertShaderResourceType(ShaderResourceType type)
    {
        switch (type)
        {
            default:
            { FL_ASSERT(false, "Vulkan does not support specified ShaderResourceType"); } break;
            case ShaderResourceType::Texture: return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            case ShaderResourceType::StorageTexture: return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            case ShaderResourceType::UniformBuffer: return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
            case ShaderResourceType::StorageBuffer: return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
            case ShaderResourceType::SubpassInput: return VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
            case ShaderResourceType::AccelerationStructure: return VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
        }

        return VK_DESCRIPTOR_TYPE_MAX_ENUM;
    }

    ShaderResourceType Common::RevertShaderResourceType(VkDescriptorType type)
    {
        switch (type)
        {
            default:
            { FL_ASSERT(false, "Vulkan does not support specified ShaderResourceType"); } break;
            case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER: return ShaderResourceType::Texture;
            case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE: return ShaderResourceType::StorageTexture;
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC: return ShaderResourceType::UniformBuffer;
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC: return ShaderResourceType::StorageBuffer;
            case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT: return ShaderResourceType::SubpassInput;
            case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR: return ShaderResourceType::AccelerationStructure;
        }

        return ShaderResourceType::None;
    }

    VkShaderStageFlags Common::ConvertShaderAccessType(ShaderType type)
    {
        VkShaderStageFlags result = 0;
        result |= ((type & ShaderTypeFlags::Vertex) > 0) * VK_SHADER_STAGE_VERTEX_BIT;
        result |= ((type & ShaderTypeFlags::Fragment) > 0) * VK_SHADER_STAGE_FRAGMENT_BIT;
        result |= ((type & ShaderTypeFlags::Compute) > 0) * VK_SHADER_STAGE_COMPUTE_BIT;
        result |= ((type & ShaderTypeFlags::RayGen) > 0) * VK_SHADER_STAGE_RAYGEN_BIT_KHR;
        result |= ((type & ShaderTypeFlags::RayMiss) > 0) * VK_SHADER_STAGE_MISS_BIT_KHR;
        result |= ((type & ShaderTypeFlags::RayIntersection) > 0) * VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
        result |= ((type & ShaderTypeFlags::RayClosestHit) > 0) * VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
        result |= ((type & ShaderTypeFlags::RayAnyHit) > 0) * VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
        return result;
    }

    VkBlendFactor Common::ConvertBlendFactor(BlendFactor factor)
    {
        switch (factor)
        {
            default:
            { FL_ASSERT(false, "Vulkan does not support specified BlendFactor"); } break;
            case BlendFactor::Zero: return VK_BLEND_FACTOR_ZERO;
            case BlendFactor::One: return VK_BLEND_FACTOR_ONE;

            case BlendFactor::SrcColor: return VK_BLEND_FACTOR_SRC_COLOR;
            case BlendFactor::OneMinusSrcColor: return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
            case BlendFactor::DstColor: return VK_BLEND_FACTOR_DST_COLOR;
            case BlendFactor::OneMinusDstColor: return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;

            case BlendFactor::SrcAlpha: return VK_BLEND_FACTOR_SRC_ALPHA;
            case BlendFactor::OneMinusSrcAlpha: return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            case BlendFactor::DstAlpha: return VK_BLEND_FACTOR_DST_ALPHA;
            case BlendFactor::OneMinusDstAlpha: return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
        }

        return VK_BLEND_FACTOR_MAX_ENUM;
    }

    VkBlendOp Common::ConvertBlendOperation(BlendOperation op)
    {
        switch (op)
        {
            default:
            { FL_ASSERT(false, "Vulkan does not support specified BlendOperation"); } break;
            case BlendOperation::Add: return VK_BLEND_OP_ADD;
            case BlendOperation::Subtract: return VK_BLEND_OP_SUBTRACT;
            case BlendOperation::ReverseSubtract: return VK_BLEND_OP_REVERSE_SUBTRACT;
            case BlendOperation::Min: return VK_BLEND_OP_MIN;
            case BlendOperation::Max: return VK_BLEND_OP_MAX;
        }

        return VK_BLEND_OP_MAX_ENUM;
    }

    VkFilter Common::ConvertSamplerFilter(SamplerFilter filter)
    {
        switch (filter)
        {
            default:
            { FL_ASSERT(false, "Vulkan does not support specified SamplerFilter"); } break;
            case SamplerFilter::Linear: return VK_FILTER_LINEAR;
            case SamplerFilter::Nearest: return VK_FILTER_NEAREST;
        }

        return VK_FILTER_MAX_ENUM;
    }

    VkSamplerAddressMode Common::ConvertSamplerWrapMode(SamplerWrapMode mode)
    {
        switch (mode)
        {
            default:
            { FL_ASSERT(false, "Vulkan does not support specified SamplerWrapMode"); } break;
            case SamplerWrapMode::ClampToBorder: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
            case SamplerWrapMode::ClampToEdge: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            case SamplerWrapMode::Repeat: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
            case SamplerWrapMode::MirroredRepeat: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        }

        return VK_SAMPLER_ADDRESS_MODE_MAX_ENUM;
    }

    VkSamplerReductionMode Common::ConvertSamplerReductionMode(SamplerReductionMode mode)
    {
        switch (mode)
        {
            default:
            { FL_ASSERT(false, "Vulkan does not support specified SamplerReductionMode"); } break;
            case SamplerReductionMode::WeightedAverage: return VK_SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE;
            case SamplerReductionMode::Min: return VK_SAMPLER_REDUCTION_MODE_MIN;
            case SamplerReductionMode::Max: return VK_SAMPLER_REDUCTION_MODE_MAX;
        }

        return VK_SAMPLER_REDUCTION_MODE_MAX_ENUM;
    }

    VkAccelerationStructureTypeKHR Common::ConvertAccelerationStructureType(AccelerationStructureType type)
    {
        switch (type)
        {
            default:
            { FL_ASSERT(false, "Vulkan does not support specified AccelerationStructureType"); } break;
            case AccelerationStructureType::Node: return VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
            case AccelerationStructureType::Scene: return VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        }

        return VK_ACCELERATION_STRUCTURE_TYPE_MAX_ENUM_KHR;
    }

    VkBuildAccelerationStructureFlagsKHR Common::ConvertAccelerationStructurePerformanceType(AccelerationStructurePerformanceType type)
    {
        switch (type)
        {
            default:
            { FL_ASSERT(false, "Vulkan does not support specified AccelerationStructurePerformanceType"); } break;
            case AccelerationStructurePerformanceType::FasterRuntime: return VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
            case AccelerationStructurePerformanceType::FasterBuilds: return VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR;
        }

        return VK_BUILD_ACCELERATION_STRUCTURE_FLAG_BITS_MAX_ENUM_KHR;
    }

    bool Common::CheckResult(VkResult result, bool ensure, const char* name)
    {
        /*
        #if defined(FL_DEBUG)
        if (result == VK_ERROR_DEVICE_LOST)
            DebugCheckpoints::LogCheckpoints();
        #endif
        */

        if (ensure)
        { FL_ASSERT(result == VK_SUCCESS, "%s critically failed with error %d", name, result); }
        else
        { FL_ASSERT(result == VK_SUCCESS, "%s failed with error %d", name, result); }
        
        return result == VK_SUCCESS;
    }
}
