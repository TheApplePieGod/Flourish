#include "flpch.h"
#include "Shader.h"

#include "Flourish/Backends/Vulkan/Context.h"
#include "Flourish/Backends/Vulkan/Util/DescriptorPool.h"
#include "Flourish/Backends/Vulkan/ResourceSet.h"
#include "shaderc/shaderc.hpp"
#include "spirv_cross/spirv_glsl.hpp"

namespace Flourish::Vulkan
{
    std::string ReadFileToString(std::filesystem::path path)
    {
        std::ifstream file(path, std::ios::ate | std::ios::binary);
        if (!file.is_open())
            return "";
        
        u32 fileSize = static_cast<u32>(file.tellg());
        std::vector<char> buffer(fileSize);
        file.seekg(0, std::ios::beg);
        file.read(buffer.data(), fileSize);
        file.close();

        return std::string(buffer.data(), buffer.size());
    }

    class ShaderIncluder : public shaderc::CompileOptions::IncluderInterface
    {
    public:
        ShaderIncluder(std::string_view basePath)
            : m_BasePath(basePath)
        {}

        shaderc_include_result* GetInclude (
            const char* requestedSource,
            shaderc_include_type type,
            const char* requestingSource,
            size_t includeDepth)
        {
            std::string name(requestedSource);
            std::string contents = ReadFileToString(
                std::filesystem::path(m_BasePath.data()).append(requestedSource).generic_u8string()
            );

            auto container = new std::array<std::string, 2>;
            (*container)[0] = name;
            (*container)[1] = contents;

            auto data = new shaderc_include_result;

            data->user_data = container;

            data->source_name = (*container)[0].data();
            data->source_name_length = (*container)[0].size();

            data->content = (*container)[1].data();
            data->content_length = (*container)[1].size();

            return data;
        };

        void ReleaseInclude(shaderc_include_result* data) override
        {
            delete static_cast<std::array<std::string, 2>*>(data->user_data);
            delete data;
        };

    private:
        std::string m_BasePath;
    };

    std::vector<u32> CompileSpirv(std::string_view path, std::string_view source, ShaderType type)
    {
        shaderc::Compiler compiler;
		shaderc::CompileOptions options;

        #ifdef FL_DEBUG
            options.SetOptimizationLevel(shaderc_optimization_level_zero);
        #else
            options.SetOptimizationLevel(shaderc_optimization_level_performance);
        #endif

        std::string baseSource(source);
        options.SetTargetEnvironment(shaderc_target_env::shaderc_target_env_vulkan, 0);
        if (!path.empty())
        {
            // If the path is passed in, we want to enable the #include resolver
            std::string basePath = std::filesystem::path(path).parent_path().generic_u8string();
            options.SetIncluder(std::make_unique<ShaderIncluder>(basePath));

            // We also want to load the source at that path
            baseSource = ReadFileToString(path);
        }

        FL_CRASH_ASSERT(!baseSource.empty(), "Failed to load source code for shader or source code was empty");

        shaderc_shader_kind shaderKind = shaderc_glsl_vertex_shader;
        switch (type)
        {
            default:
            { FL_CRASH_ASSERT(false, "Can't compile unsupported shader type"); } break;
            case ShaderTypeFlags::Vertex: { shaderKind = shaderc_glsl_vertex_shader; } break;
            case ShaderTypeFlags::Fragment: { shaderKind = shaderc_glsl_fragment_shader; } break;
            case ShaderTypeFlags::Compute: { shaderKind = shaderc_glsl_compute_shader; } break;
            case ShaderTypeFlags::RayGen: { shaderKind = shaderc_glsl_raygen_shader; } break;
            case ShaderTypeFlags::RayMiss: { shaderKind = shaderc_glsl_miss_shader; } break;
            case ShaderTypeFlags::RayIntersection: { shaderKind = shaderc_glsl_intersection_shader; } break;
            case ShaderTypeFlags::RayClosestHit: { shaderKind = shaderc_glsl_closesthit_shader; } break;
            case ShaderTypeFlags::RayAnyHit: { shaderKind = shaderc_glsl_anyhit_shader; } break;
        }

        shaderc::PreprocessedSourceCompilationResult preprocessed = compiler.PreprocessGlsl(
            baseSource.data(),
            shaderKind,
            path.empty() ? "shader" : path.data(),
            options
        );
        FL_CRASH_ASSERT(
            preprocessed.GetCompilationStatus() == shaderc_compilation_status_success,
            "Shader preprocessing failed: %s",
            preprocessed.GetErrorMessage().data()
        );

        shaderc::SpvCompilationResult compiled = compiler.CompileGlslToSpv(
            preprocessed.begin(),
            shaderKind,
            path.empty() ? "shader" : path.data(),
            options
        );
        FL_CRASH_ASSERT(
            compiled.GetCompilationStatus() == shaderc_compilation_status_success,
            "Shader compilation failed: %s",
            compiled.GetErrorMessage().data()
        );

        return std::vector<u32>((u32*)compiled.cbegin(), (u32*)compiled.cend());
    }

    Shader::Shader(const ShaderCreateInfo& createInfo)
        : Flourish::Shader(createInfo)
    {
        std::vector<u32> compiled = CompileSpirv(createInfo.Path, createInfo.Source, createInfo.Type);
        Reflect(compiled);

        VkShaderModuleCreateInfo modCreateInfo{};
        modCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        modCreateInfo.codeSize = compiled.size() * sizeof(u32);
        modCreateInfo.pCode = compiled.data();

        if (!FL_VK_CHECK_RESULT(vkCreateShaderModule(
            Context::Devices().Device(),
            &modCreateInfo,
            nullptr,
            &m_ShaderModule
        ), "Shader create shader module"))
            throw std::exception();
    }

    Shader::~Shader()
    {
        // TODO: might not actually need to be on the queue since it's a one-time use thing
        // but to be safe it's here anyways
        auto mod = m_ShaderModule;
        Context::FinalizerQueue().Push([=]()
        {
            if (mod)
                vkDestroyShaderModule(Context::Devices().Device(), mod, nullptr);
        }, "Shader free");
    }

    VkPipelineShaderStageCreateInfo Shader::DefineShaderStage(const char* entrypoint)
    {
        VkShaderStageFlagBits stage;
        switch (m_Type)
        {
            case ShaderTypeFlags::Vertex: { stage = VK_SHADER_STAGE_VERTEX_BIT; } break;
            case ShaderTypeFlags::Fragment: { stage = VK_SHADER_STAGE_FRAGMENT_BIT; } break;
            case ShaderTypeFlags::Compute: { stage = VK_SHADER_STAGE_COMPUTE_BIT; } break;
            case ShaderTypeFlags::RayGen: { stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR; } break;
            case ShaderTypeFlags::RayMiss: { stage = VK_SHADER_STAGE_MISS_BIT_KHR; } break;
            case ShaderTypeFlags::RayIntersection: { stage = VK_SHADER_STAGE_INTERSECTION_BIT_KHR; } break;
            case ShaderTypeFlags::RayClosestHit: { stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR; } break;
            case ShaderTypeFlags::RayAnyHit: { stage = VK_SHADER_STAGE_ANY_HIT_BIT_KHR; } break;
        }

        VkPipelineShaderStageCreateInfo shaderStageInfo{};
        shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStageInfo.stage = stage;
        shaderStageInfo.module = m_ShaderModule;
        shaderStageInfo.pName = entrypoint;
        shaderStageInfo.pSpecializationInfo = nullptr;

        return shaderStageInfo;
    }

    void Shader::Reflect(const std::vector<u32>& compiledData)
    {
        m_ReflectionData.clear();

        u32 size = compiledData.size();

        // For some reason, compiler needs to be heap allocated because otherwise it causes
        // a crash on macos
        auto compiler = std::make_unique<spirv_cross::Compiler>(compiledData.data(), compiledData.size());
        
        // Again on macos something is broken, so we need to zero out these struct members otherwise it crashes
		spirv_cross::ShaderResources resources = compiler->get_shader_resources();
        memset(&resources.builtin_inputs, 0, sizeof(resources.builtin_inputs));
        memset(&resources.builtin_outputs, 0, sizeof(resources.builtin_outputs));

        const char* typeString = "";
        switch (m_Type)
        {
            case ShaderTypeFlags::Vertex: { typeString = "Vertex"; } break;
            case ShaderTypeFlags::Fragment: { typeString = "Fragment"; } break;
            case ShaderTypeFlags::Compute: { typeString = "Compute"; } break;
            case ShaderTypeFlags::RayGen: { typeString = "RayGen"; } break;
            case ShaderTypeFlags::RayMiss: { typeString = "RayMiss"; } break;
            case ShaderTypeFlags::RayIntersection: { typeString = "RayIntersection"; } break;
            case ShaderTypeFlags::RayClosestHit: { typeString = "RayClosestHit"; } break;
            case ShaderTypeFlags::RayAnyHit: { typeString = "RayAnyHit"; } break;
        }

		FL_LOG_DEBUG("GLSL %s shader", typeString);
		FL_LOG_DEBUG("    %d uniform buffers", resources.uniform_buffers.size());
        FL_LOG_DEBUG("    %d storage buffers", resources.storage_buffers.size());
		FL_LOG_DEBUG("    %d sampled images", resources.sampled_images.size());
        FL_LOG_DEBUG("    %d storage images", resources.storage_images.size());
        FL_LOG_DEBUG("    %d subpass inputs", resources.subpass_inputs.size());

        u32 maxSets = Context::Devices().PhysicalDeviceProperties().limits.maxBoundDescriptorSets;

        /*
         * TODO: Clean this up
         */

        if (resources.uniform_buffers.size() > 0)
		    FL_LOG_DEBUG("  Uniform buffers:");
		for (const auto& resource : resources.uniform_buffers)
		{
			const auto& bufferType = compiler->get_type(resource.base_type_id);
			size_t bufferSize = compiler->get_declared_struct_size(bufferType);
			u32 binding = compiler->get_decoration(resource.id, spv::DecorationBinding);
            u32 set = compiler->get_decoration(resource.id, spv::DecorationDescriptorSet);
			size_t memberCount = bufferType.member_types.size();

            m_ReflectionData.emplace_back(
                ShaderResourceType::UniformBuffer,
                m_Type,
                binding,
                set,
                (u32)bufferSize, 1
            );

			FL_LOG_DEBUG("    %s", resource.name.c_str());
			FL_LOG_DEBUG("      Size = %d", bufferSize);
            FL_LOG_DEBUG("      Set = %d", set);
			FL_LOG_DEBUG("      Binding = %d", binding);
			FL_LOG_DEBUG("      Members = %d", memberCount);

            if (set >= maxSets)
            {
                FL_LOG_ERROR("Failed to initialize shader, the 'set' qualifier must be less than %d but is %d on uniform %s", maxSets, set, resource.name.c_str());
                throw std::exception();
            }
		}

        if (resources.storage_buffers.size() > 0)
		    FL_LOG_DEBUG("  Storage buffers:");
		for (const auto& resource : resources.storage_buffers)
		{
			const auto& bufferType = compiler->get_type(resource.base_type_id);
			size_t bufferSize = compiler->get_declared_struct_size(bufferType);
			u32 binding = compiler->get_decoration(resource.id, spv::DecorationBinding);
            u32 set = compiler->get_decoration(resource.id, spv::DecorationDescriptorSet);
			size_t memberCount = bufferType.member_types.size();

            m_ReflectionData.emplace_back(
                ShaderResourceType::StorageBuffer,
                m_Type,
                binding,
                set,
                (u32)bufferSize, 1
            );

			FL_LOG_DEBUG("    %s", resource.name.c_str());
			FL_LOG_DEBUG("      Size = %d", bufferSize);
            FL_LOG_DEBUG("      Set = %d", set);
			FL_LOG_DEBUG("      Binding = %d", binding);
			FL_LOG_DEBUG("      Members = %d", memberCount);

            if (set >= maxSets)
            {
                FL_LOG_ERROR("Failed to initialize shader, the 'set' qualifier must be less than %d but is %d on storage %s", maxSets, set, resource.name.c_str());
                throw std::exception();
            }
		}

        if (resources.sampled_images.size() > 0)
		    FL_LOG_DEBUG("  Sampled Images:");
		for (const auto& resource : resources.sampled_images)
		{
            const auto& imageType = compiler->get_type(resource.type_id);
			u32 binding = compiler->get_decoration(resource.id, spv::DecorationBinding);
            u32 set = compiler->get_decoration(resource.id, spv::DecorationDescriptorSet);
            u32 arrayCount = imageType.array.empty() ? 1 : imageType.array[0];
            if (imageType.image.dim == spv::Dim::DimCube)
                arrayCount *= 6;
            
            m_ReflectionData.emplace_back(
                ShaderResourceType::Texture,
                m_Type,
                binding,
                set, 0,
                arrayCount
            );

			FL_LOG_DEBUG("    Image (%s)", resource.name.c_str());
            FL_LOG_DEBUG("      ArrayCount = %d", arrayCount);
            FL_LOG_DEBUG("      Set = %d", set);
			FL_LOG_DEBUG("      Binding = %d", binding);

            if (set >= maxSets)
            {
                FL_LOG_ERROR("Failed to initialize shader, the 'set' qualifier must be less than %d but is %d on sampler %s", maxSets, set, resource.name.c_str());
                throw std::exception();
            }
		}

        if (resources.storage_images.size() > 0)
		    FL_LOG_DEBUG("  Storage Images:");
		for (const auto& resource : resources.storage_images)
		{
            const auto& imageType = compiler->get_type(resource.type_id);
			u32 binding = compiler->get_decoration(resource.id, spv::DecorationBinding);
            u32 set = compiler->get_decoration(resource.id, spv::DecorationDescriptorSet);
            u32 arrayCount = imageType.array.empty() ? 1 : imageType.array[0];
            if (imageType.image.dim == spv::Dim::DimCube)
                arrayCount *= 6;
            
            m_ReflectionData.emplace_back(
                ShaderResourceType::StorageTexture,
                m_Type,
                binding,
                set, 0,
                arrayCount
            );

			FL_LOG_DEBUG("    StorageImage (%s)", resource.name.c_str());
            FL_LOG_DEBUG("      ArrayCount = %d", arrayCount);
            FL_LOG_DEBUG("      Set = %d", set);
			FL_LOG_DEBUG("      Binding = %d", binding);

            if (set >= maxSets)
            {
                FL_LOG_ERROR("Failed to initialize shader, the 'set' qualifier must be less than %d but is %d on image %s", maxSets, set, resource.name.c_str());
                throw std::exception();
            }
		}

        if (resources.subpass_inputs.size() > 0)
		    FL_LOG_DEBUG("  Subpass Inputs:");
		for (const auto& resource : resources.subpass_inputs)
		{
            const auto& subpassType = compiler->get_type(resource.type_id);
			u32 binding = compiler->get_decoration(resource.id, spv::DecorationBinding);
            u32 set = compiler->get_decoration(resource.id, spv::DecorationDescriptorSet);
            u32 attachmentIndex = compiler->get_decoration(resource.id, spv::DecorationInputAttachmentIndex);
            
            m_ReflectionData.emplace_back(
                ShaderResourceType::SubpassInput,
                m_Type,
                binding,
                set, 0, 1
            );

			FL_LOG_DEBUG("    Input (%s)", resource.name.c_str());
            FL_LOG_DEBUG("      Set = %d", set);
			FL_LOG_DEBUG("      Binding = %d", binding);
            FL_LOG_DEBUG("      AttachmentIndex = %d", attachmentIndex);

            if (set >= maxSets)
            {
                FL_LOG_ERROR("Failed to initialize shader, the 'set' qualifier must be less than %d but is %d on subpass %s", maxSets, set, resource.name.c_str());
                throw std::exception();
            }
		}

        if (resources.acceleration_structures.size() > 0)
		    FL_LOG_DEBUG("  Acceleration Structures:");
		for (const auto& resource : resources.acceleration_structures)
		{
            const auto& accelType = compiler->get_type(resource.type_id);
			u32 binding = compiler->get_decoration(resource.id, spv::DecorationBinding);
            u32 set = compiler->get_decoration(resource.id, spv::DecorationDescriptorSet);
            
            m_ReflectionData.emplace_back(
                ShaderResourceType::AccelerationStructure,
                m_Type,
                binding,
                set, 0, 1
            );

			FL_LOG_DEBUG("    Accel Struct (%s)", resource.name.c_str());
            FL_LOG_DEBUG("      Set = %d", set);
			FL_LOG_DEBUG("      Binding = %d", binding);

            if (set >= maxSets)
            {
                FL_LOG_ERROR("Failed to initialize shader, the 'set' qualifier must be less than %d but is %d on accel struct %s", maxSets, set, resource.name.c_str());
                throw std::exception();
            }
		}
    }
}
