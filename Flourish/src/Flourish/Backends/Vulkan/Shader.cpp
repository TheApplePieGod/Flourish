#include "flpch.h"
#include "Shader.h"

#include "Flourish/Backends/Vulkan/Context.h"
#include "Flourish/Backends/Vulkan/Util/DescriptorPool.h"
#include "Flourish/Backends/Vulkan/ResourceSet.h"
#include "shaderc/shaderc.hpp"
#include "spirv_cross/spirv_cross_c.h"

namespace Flourish::Vulkan
{
    std::string ReadFileToString(std::filesystem::path path)
    {
        u32 outLength;
        unsigned char* data = Flourish::Context::ReadFile()(path.generic_u8string(), outLength);
        if (!data)
            return "";
        auto str = std::string((char*)data, outLength);
        delete[] data;
        return str;
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
            size_t includeDepth) override
        {
            if (includeDepth > 30)
            {
                FL_LOG_ERROR("Shader maximum include depth exceeded: %s", requestingSource);
                throw std::exception();
            }

            // TODO: janky and only supports depth 1 inclusions
            std::filesystem::path loadPath = std::filesystem::path(m_BasePath);
            if (includeDepth > 1)
                loadPath.append(std::filesystem::path(requestingSource).parent_path().generic_u8string());
            loadPath.append(requestedSource);

            std::string name(requestedSource);
            std::string contents = ReadFileToString(loadPath.generic_u8string());
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
            options.SetGenerateDebugInfo();
        #else
            options.SetOptimizationLevel(shaderc_optimization_level_performance);
        #endif

        // Set target compilation environment depending on which spirv version we support
        options.SetTargetEnvironment(shaderc_target_env::shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_1);
        if (Context::Devices().SupportsSpirv14())
        {
            options.SetTargetEnvironment(shaderc_target_env::shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_2);
            options.SetTargetSpirv(shaderc_spirv_version_1_4);
        }

        std::string baseSource(source);
        if (!path.empty())
        {
            // If the path is passed in, we want to enable the #include resolver
            std::string basePath = std::filesystem::path(path).parent_path().generic_u8string();
            options.SetIncluder(std::make_unique<ShaderIncluder>(basePath));

            // We also want to load the source at that path
            baseSource = ReadFileToString(path);
        }

        if (baseSource.empty())
        {
            FL_LOG_ERROR("Failed to load source code for shader or source code was empty");
            return std::vector<u32>();
        }

        shaderc_shader_kind shaderKind = shaderc_glsl_vertex_shader;
        switch (type)
        {
            default:
            {
                FL_LOG_ERROR("Can't compile unsupported shader type");
                return std::vector<u32>();
            } break;
            case ShaderTypeFlags::Vertex: { shaderKind = shaderc_glsl_vertex_shader; } break;
            case ShaderTypeFlags::Fragment: { shaderKind = shaderc_glsl_fragment_shader; } break;
            case ShaderTypeFlags::Compute: { shaderKind = shaderc_glsl_compute_shader; } break;
            case ShaderTypeFlags::RayGen: { shaderKind = shaderc_glsl_raygen_shader; } break;
            case ShaderTypeFlags::RayMiss: { shaderKind = shaderc_glsl_miss_shader; } break;
            case ShaderTypeFlags::RayIntersection: { shaderKind = shaderc_glsl_intersection_shader; } break;
            case ShaderTypeFlags::RayClosestHit: { shaderKind = shaderc_glsl_closesthit_shader; } break;
            case ShaderTypeFlags::RayAnyHit: { shaderKind = shaderc_glsl_anyhit_shader; } break;
        }

        // Inject macro definitions for certain enabled features
        if (Flourish::Context::FeatureTable().RayTracing)
            options.AddMacroDefinition("FLOURISH_RAY_TRACING");

        shaderc::SpvCompilationResult compiled = compiler.CompileGlslToSpv(
            baseSource.c_str(),
            baseSource.size(),
            shaderKind,
            path.empty() ? "shader" : path.data(),
            options
        );

        if (compiled.GetCompilationStatus() != shaderc_compilation_status_success)
        {
            FL_LOG_ERROR(
                "Shader compilation failed with code %d: %s",
                compiled.GetCompilationStatus(),
                compiled.GetErrorMessage().data()
            );
            return std::vector<u32>();
        }

        return std::vector<u32>((u32*)compiled.cbegin(), (u32*)compiled.cend());
    }

    Shader::Shader(const ShaderCreateInfo& createInfo)
        : Flourish::Shader(createInfo)
    {
        Recreate();

        // Failed to compile
        if (!m_Revisions)
            FL_CRASH_ASSERT(false, "Failed to create shader because initial compilation failed");
    }

    Shader::~Shader()
    {
        Cleanup();
    }

    bool Shader::Reload()
    {
        return Recreate();
    }

    // returns true if the recreation was successful
    bool Shader::Recreate()
    {
        if (m_Revisions)
        {
            // Recreating from source doesnt make sense because the source
            // is immutable
            if (m_Info.Path.empty())
                return true;
            
            FL_LOG_DEBUG("Recompiling shader @ '%s'", m_Info.Path.data());
        }

        std::vector<u32> compiled = CompileSpirv(m_Info.Path, m_Info.Source, m_Info.Type);
        if (compiled.empty()) // Compilation failure
            return false;

        Cleanup();

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
            return false;

        m_Revisions++;

        return true;
    }

    void Shader::Cleanup()
    {
        // TODO: might not actually need to be on the queue since it's a one-time use thing
        // but to be safe it's here anyways
        auto mod = m_ShaderModule;
        Context::FinalizerQueue().Push([=]()
        {
            if (mod)
                vkDestroyShaderModule(Context::Devices().Device(), mod, nullptr);
        }, "Shader free");

        m_ShaderModule = VK_NULL_HANDLE;
    }

    VkPipelineShaderStageCreateInfo Shader::DefineShaderStage(const char* entrypoint)
    {
        VkShaderStageFlagBits stage;
        switch (m_Info.Type)
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

    bool Shader::CheckSpecializationCompatability(const SpecializationConstant& spec)
    {
        for (auto& trueSpec : m_SpecializationReflection)
            if (trueSpec.ConstantId == spec.ConstantId && trueSpec.Type == spec.Type)
                return true;
        return false;
    }

    void Shader::Reflect(const std::vector<u32>& compiledData)
    {
        m_ReflectionData.clear();
        m_SpecializationReflection.clear();

        u32 size = compiledData.size();

        // Create context
        spvc_context context = nullptr;
        spvc_context_create(&context);
        
        // Parse spirv
        spvc_parsed_ir ir = nullptr;
        spvc_context_parse_spirv(context, compiledData.data(), compiledData.size(), &ir);

        // Create compiler
        spvc_compiler compiler = nullptr;
        spvc_context_create_compiler(context, SPVC_BACKEND_GLSL, ir, SPVC_CAPTURE_MODE_TAKE_OWNERSHIP, &compiler);

        // Get resources
        spvc_resources resources = nullptr;
        spvc_compiler_create_shader_resources(compiler, &resources);

        // Reflect
        const spvc_reflected_resource* uniforms = nullptr;
        size_t uniformsCount = 0;
        spvc_resources_get_resource_list_for_type(resources, SPVC_RESOURCE_TYPE_UNIFORM_BUFFER, &uniforms, &uniformsCount);
        const spvc_reflected_resource* storages = nullptr;
        size_t storagesCount = 0;
        spvc_resources_get_resource_list_for_type(resources, SPVC_RESOURCE_TYPE_STORAGE_BUFFER, &storages, &storagesCount);
        const spvc_reflected_resource* sampledImages = nullptr;
        size_t sampledImagesCount = 0;
        spvc_resources_get_resource_list_for_type(resources, SPVC_RESOURCE_TYPE_SAMPLED_IMAGE, &sampledImages, &sampledImagesCount);
        const spvc_reflected_resource* storageImages = nullptr;
        size_t storageImagesCount = 0;
        spvc_resources_get_resource_list_for_type(resources, SPVC_RESOURCE_TYPE_STORAGE_IMAGE, &storageImages, &storageImagesCount);
        const spvc_reflected_resource* subpassInputs = nullptr;
        size_t subpassInputsCount = 0;
        spvc_resources_get_resource_list_for_type(resources, SPVC_RESOURCE_TYPE_SUBPASS_INPUT, &subpassInputs, &subpassInputsCount);
        const spvc_reflected_resource* accelStructs = nullptr;
        size_t accelStructsCount = 0;
        spvc_resources_get_resource_list_for_type(resources, SPVC_RESOURCE_TYPE_ACCELERATION_STRUCTURE, &accelStructs, &accelStructsCount);
        const spvc_reflected_resource* pushConstants = nullptr;
        size_t pushConstantsCount = 0;
        spvc_resources_get_resource_list_for_type(resources, SPVC_RESOURCE_TYPE_PUSH_CONSTANT, &pushConstants, &pushConstantsCount);

        const char* typeString = "";
        switch (m_Info.Type)
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
		FL_LOG_DEBUG("    %d uniform buffers", uniformsCount);
        FL_LOG_DEBUG("    %d storage buffers", storagesCount);
		FL_LOG_DEBUG("    %d sampled images", sampledImagesCount);
        FL_LOG_DEBUG("    %d storage images", storageImagesCount);
        FL_LOG_DEBUG("    %d subpass inputs", subpassInputsCount);
        FL_LOG_DEBUG("    %d acceleration structs", accelStructsCount);

        /*
         * Reflect specialization constants
         */

        const spvc_specialization_constant* specConstants = nullptr;
        size_t specConstantsCount = 0;
        spvc_compiler_get_specialization_constants(compiler, &specConstants, &specConstantsCount);
        for (size_t i = 0; i < specConstantsCount; i++)
        {
            m_SpecializationReflection.emplace_back();

            auto handle = spvc_compiler_get_constant_handle(compiler, specConstants[i].id);
            auto typeId = spvc_constant_get_type(handle);
            auto type = spvc_compiler_get_type_handle(compiler, typeId);
            auto baseType = spvc_type_get_basetype(type);

            m_SpecializationReflection.back().ConstantId = specConstants[i].constant_id;

            switch (baseType)
            {
                default:
                {
                    FL_LOG_ERROR("Shader has unsupported specialization constant data type");
                    spvc_context_destroy(context);
                    throw std::exception();
                    return;
                }
                case SPVC_BASETYPE_INT32: { m_SpecializationReflection.back().Type = SpecializationConstantType::Int; } break;
                case SPVC_BASETYPE_UINT32: { m_SpecializationReflection.back().Type = SpecializationConstantType::UInt; } break;
                case SPVC_BASETYPE_FP32: { m_SpecializationReflection.back().Type = SpecializationConstantType::Float; } break;
                case SPVC_BASETYPE_BOOLEAN: { m_SpecializationReflection.back().Type = SpecializationConstantType::Bool; } break;
            }

            // All supported types are 4 bytes
            m_TotalSpecializationSize += 4;
        }

        /*
         * Reflect shader resources
         * TODO: Clean this up
         */

        u32 maxSets = Context::Devices().PhysicalDeviceProperties().limits.maxBoundDescriptorSets;
        if (uniformsCount)
		    FL_LOG_DEBUG("  Uniform buffers:");
		for (size_t i = 0; i < uniformsCount; i++)
		{
            auto bufferType = spvc_compiler_get_type_handle(compiler, uniforms[i].base_type_id);
            size_t bufferSize = 0;
            spvc_compiler_get_declared_struct_size(compiler, bufferType, &bufferSize);
            u32 binding = spvc_compiler_get_decoration(compiler, uniforms[i].id, SpvDecorationBinding);
            u32 set = spvc_compiler_get_decoration(compiler, uniforms[i].id, SpvDecorationDescriptorSet);
            u32 memberCount = spvc_type_get_num_member_types(bufferType);

            m_ReflectionData.emplace_back(
                ShaderResourceType::UniformBuffer,
                m_Info.Type,
                binding,
                set,
                (u32)bufferSize, 1
            );

			FL_LOG_DEBUG("    %s", uniforms[i].name);
			FL_LOG_DEBUG("      Size = %d", bufferSize);
            FL_LOG_DEBUG("      Set = %d", set);
			FL_LOG_DEBUG("      Binding = %d", binding);
			FL_LOG_DEBUG("      Members = %d", memberCount);

            if (set >= maxSets)
            {
                FL_LOG_ERROR("Failed to initialize shader, the 'set' qualifier must be less than %d but is %d on uniform %s", maxSets, set, uniforms[i].name);
                spvc_context_destroy(context);
                throw std::exception();
                return;
            }
		}

        if (storagesCount)
		    FL_LOG_DEBUG("  Storage buffers:");
		for (size_t i = 0; i < storagesCount; i++)
		{
            auto bufferType = spvc_compiler_get_type_handle(compiler, storages[i].base_type_id);
            size_t bufferSize = 0;
            spvc_compiler_get_declared_struct_size(compiler, bufferType, &bufferSize);
            u32 binding = spvc_compiler_get_decoration(compiler, storages[i].id, SpvDecorationBinding);
            u32 set = spvc_compiler_get_decoration(compiler, storages[i].id, SpvDecorationDescriptorSet);
            u32 memberCount = spvc_type_get_num_member_types(bufferType);

            m_ReflectionData.emplace_back(
                ShaderResourceType::StorageBuffer,
                m_Info.Type,
                binding,
                set,
                (u32)bufferSize, 1
            );

			FL_LOG_DEBUG("    %s", storages[i].name);
			FL_LOG_DEBUG("      Size = %d", bufferSize);
            FL_LOG_DEBUG("      Set = %d", set);
			FL_LOG_DEBUG("      Binding = %d", binding);
			FL_LOG_DEBUG("      Members = %d", memberCount);

            if (set >= maxSets)
            {
                FL_LOG_ERROR("Failed to initialize shader, the 'set' qualifier must be less than %d but is %d on storage %s", maxSets, set, storages[i].name);
                spvc_context_destroy(context);
                throw std::exception();
                return;
            }
		}

        if (sampledImagesCount)
		    FL_LOG_DEBUG("  Sampled Images:");
		for (size_t i = 0; i < sampledImagesCount; i++)
		{
            auto imageType = spvc_compiler_get_type_handle(compiler, sampledImages[i].type_id);
            u32 binding = spvc_compiler_get_decoration(compiler, sampledImages[i].id, SpvDecorationBinding);
            u32 set = spvc_compiler_get_decoration(compiler, sampledImages[i].id, SpvDecorationDescriptorSet);
            u32 arrayDimension = spvc_type_get_num_array_dimensions(imageType);
            u32 arrayCount = arrayDimension == 0 ? 1 : spvc_type_get_array_dimension(imageType, 0);
            
            m_ReflectionData.emplace_back(
                ShaderResourceType::Texture,
                m_Info.Type,
                binding,
                set, 0,
                arrayCount
            );

			FL_LOG_DEBUG("    Image (%s)", sampledImages[i].name);
            FL_LOG_DEBUG("      ArrayCount = %d", arrayCount);
            FL_LOG_DEBUG("      Set = %d", set);
			FL_LOG_DEBUG("      Binding = %d", binding);

            if (set >= maxSets)
            {
                FL_LOG_ERROR("Failed to initialize shader, the 'set' qualifier must be less than %d but is %d on sampler %s", maxSets, set, sampledImages[i].name);
                spvc_context_destroy(context);
                throw std::exception();
                return;
            }
		}

        if (storageImagesCount)
		    FL_LOG_DEBUG("  Storage Images:");
		for (size_t i = 0; i < storageImagesCount; i++)
		{
            auto imageType = spvc_compiler_get_type_handle(compiler, storageImages[i].type_id);
            u32 binding = spvc_compiler_get_decoration(compiler, storageImages[i].id, SpvDecorationBinding);
            u32 set = spvc_compiler_get_decoration(compiler, storageImages[i].id, SpvDecorationDescriptorSet);
            u32 arrayDimension = spvc_type_get_num_array_dimensions(imageType);
            u32 arrayCount = arrayDimension == 0 ? 1 : spvc_type_get_array_dimension(imageType, 0);
            
            m_ReflectionData.emplace_back(
                ShaderResourceType::StorageTexture,
                m_Info.Type,
                binding,
                set, 0,
                arrayCount
            );

			FL_LOG_DEBUG("    StorageImage (%s)", storageImages[i].name);
            FL_LOG_DEBUG("      ArrayCount = %d", arrayCount);
            FL_LOG_DEBUG("      Set = %d", set);
			FL_LOG_DEBUG("      Binding = %d", binding);

            if (set >= maxSets)
            {
                FL_LOG_ERROR("Failed to initialize shader, the 'set' qualifier must be less than %d but is %d on image %s", maxSets, set, storageImages[i].name);
                spvc_context_destroy(context);
                throw std::exception();
                return;
            }
		}

        if (subpassInputsCount)
		    FL_LOG_DEBUG("  Subpass Inputs:");
		for (size_t i = 0; i < subpassInputsCount; i++)
		{
            u32 binding = spvc_compiler_get_decoration(compiler, subpassInputs[i].id, SpvDecorationBinding);
            u32 set = spvc_compiler_get_decoration(compiler, subpassInputs[i].id, SpvDecorationDescriptorSet);
            u32 attachmentIndex = spvc_compiler_get_decoration(compiler, subpassInputs[i].id, SpvDecorationInputAttachmentIndex);
            
            m_ReflectionData.emplace_back(
                ShaderResourceType::SubpassInput,
                m_Info.Type,
                binding,
                set, 0, 1
            );

			FL_LOG_DEBUG("    Input (%s)", subpassInputs[i].name);
            FL_LOG_DEBUG("      Set = %d", set);
			FL_LOG_DEBUG("      Binding = %d", binding);
            FL_LOG_DEBUG("      AttachmentIndex = %d", attachmentIndex);

            if (set >= maxSets)
            {
                FL_LOG_ERROR("Failed to initialize shader, the 'set' qualifier must be less than %d but is %d on subpass %s", maxSets, set, subpassInputs[i].name);
                spvc_context_destroy(context);
                throw std::exception();
                return;
            }
		}

        if (accelStructsCount)
		    FL_LOG_DEBUG("  Acceleration Structures:");
		for (size_t i = 0; i < accelStructsCount; i++)
		{
            u32 binding = spvc_compiler_get_decoration(compiler, accelStructs[i].id, SpvDecorationBinding);
            u32 set = spvc_compiler_get_decoration(compiler, accelStructs[i].id, SpvDecorationDescriptorSet);
            
            m_ReflectionData.emplace_back(
                ShaderResourceType::AccelerationStructure,
                m_Info.Type,
                binding,
                set, 0, 1
            );

			FL_LOG_DEBUG("    Accel Struct (%s)", accelStructs[i].name);
            FL_LOG_DEBUG("      Set = %d", set);
			FL_LOG_DEBUG("      Binding = %d", binding);

            if (set >= maxSets)
            {
                FL_LOG_ERROR("Failed to initialize shader, the 'set' qualifier must be less than %d but is %d on accel struct %s", maxSets, set, accelStructs[i].name);
                spvc_context_destroy(context);
                throw std::exception();
                return;
            }
		}

        if (pushConstantsCount)
        {
		    FL_LOG_DEBUG("  Push constants:");
            FL_ASSERT(
                pushConstantsCount == 1,
                "Cannot declare more than one push constant block in a shader"
            );
        }
		for (size_t i = 0; i < pushConstantsCount; i++)
		{
            auto bufferType = spvc_compiler_get_type_handle(compiler, pushConstants[i].base_type_id);
            size_t bufferSize = 0;
            spvc_compiler_get_declared_struct_size(compiler, bufferType, &bufferSize);
            u32 memberCount = spvc_type_get_num_member_types(bufferType);

            m_PushConstantReflection.Size = bufferSize;
            m_PushConstantReflection.AccessType = m_Info.Type;

			FL_LOG_DEBUG("    %s", pushConstants[i].name);
			FL_LOG_DEBUG("      Size = %d", bufferSize);
			FL_LOG_DEBUG("      Members = %d", memberCount);
		}

        spvc_context_destroy(context);
    }
}
