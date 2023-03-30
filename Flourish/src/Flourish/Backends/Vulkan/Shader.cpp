#include "flpch.h"
#include "Shader.h"

#include "Flourish/Backends/Vulkan/Context.h"
#include "Flourish/Backends/Vulkan/Util/DescriptorPool.h"
#include "Flourish/Backends/Vulkan/DescriptorSet.h"
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
            case ShaderType::Vertex: { shaderKind = shaderc_glsl_vertex_shader; } break;
            case ShaderType::Fragment: { shaderKind = shaderc_glsl_fragment_shader; } break;
            case ShaderType::Compute: { shaderKind = shaderc_glsl_compute_shader; } break;
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

    std::shared_ptr<Flourish::DescriptorSet> Shader::CreateDescriptorSet(const DescriptorSetCreateInfo& createInfo)
    {
        u32 setIndex = createInfo.SetIndex;

        m_DescriptorPoolMutex.lock();

        if (setIndex >= m_ReflectionData.size())
        {
            FL_ASSERT(false, "AllocateDescriptorSet setIndex out of range");
            return nullptr;
        }

        if (m_SetPools.size() <= setIndex)
            m_SetPools.resize(setIndex + 1);

        if (!m_SetPools[setIndex])
            m_SetPools[setIndex] = std::make_shared<DescriptorPool>(m_ReflectionData[setIndex]);

        auto setPool = m_SetPools[setIndex];
        m_DescriptorPoolMutex.unlock();

        auto alloc = setPool->AllocateSet();
        return std::make_shared<DescriptorSet>(createInfo, alloc, setPool);
    }

    VkPipelineShaderStageCreateInfo Shader::DefineShaderStage(const char* entrypoint)
    {
        VkShaderStageFlagBits stage;
        switch (m_Type)
        {
            case ShaderType::Vertex: { stage = VK_SHADER_STAGE_VERTEX_BIT; } break;
            case ShaderType::Fragment: { stage = VK_SHADER_STAGE_FRAGMENT_BIT; } break;
            case ShaderType::Compute: { stage = VK_SHADER_STAGE_COMPUTE_BIT; } break;
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

        // For some reason, compiler needs to be heap allocated because otherwise it causes
        // a crash on macos
        auto compiler = std::make_unique<spirv_cross::Compiler>(compiledData.data(), compiledData.size());
        
        // Again on macos something is broken, so we need to zero out these struct members otherwise it crashes
		spirv_cross::ShaderResources resources = compiler->get_shader_resources();
        memset(&resources.builtin_inputs, 0, sizeof(resources.builtin_inputs));
        memset(&resources.builtin_outputs, 0, sizeof(resources.builtin_outputs));

        ShaderResourceAccessType accessType;
        /*
        switch (m_Type)
        {
            case ShaderType::Vertex: { accessType = ShaderResourceAccessType::Vertex; } break;
            case ShaderType::Fragment: { accessType = ShaderResourceAccessType::Fragment; } break;
            case ShaderType::Compute: { accessType = ShaderResourceAccessType::Compute; } break;
        }
        */
        
        // This is not great, and may potentially have performance implications.
        // We are doing this because it makes the API simpler. Potentially in the
        // future we can have a separate DescriptorSetAllocator class that can be
        // defined from reflection data but also have control over access types
        accessType = ShaderResourceAccessType::All;

        const char* typeStrings[] = {
            "None", "Vertex", "Fragment", "Compute"
        };

		FL_LOG_DEBUG("GLSL %s shader", typeStrings[static_cast<u16>(m_Type)]);
		FL_LOG_DEBUG("    %d uniform buffers", resources.uniform_buffers.size());
        FL_LOG_DEBUG("    %d storage buffers", resources.storage_buffers.size());
		FL_LOG_DEBUG("    %d sampled images", resources.sampled_images.size());
        FL_LOG_DEBUG("    %d storage images", resources.storage_images.size());
        FL_LOG_DEBUG("    %d subpass inputs", resources.subpass_inputs.size());

        u32 maxSets = Context::Devices().PhysicalDeviceProperties().limits.maxBoundDescriptorSets;

        if (resources.uniform_buffers.size() > 0)
		    FL_LOG_DEBUG("  Uniform buffers:");
		for (const auto& resource : resources.uniform_buffers)
		{
			const auto& bufferType = compiler->get_type(resource.base_type_id);
			size_t bufferSize = compiler->get_declared_struct_size(bufferType);
			u32 binding = compiler->get_decoration(resource.id, spv::DecorationBinding);
            u32 set = compiler->get_decoration(resource.id, spv::DecorationDescriptorSet);
			size_t memberCount = bufferType.member_types.size();

            while (m_ReflectionData.size() < set)
                m_ReflectionData.emplace_back();
            m_ReflectionData[set].emplace_back(resource.id, ShaderResourceType::UniformBuffer, accessType, binding, set, 1);

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

            while (m_ReflectionData.size() < set)
                m_ReflectionData.emplace_back();
            m_ReflectionData[set].emplace_back(resource.id, ShaderResourceType::StorageBuffer, accessType, binding, set, 1);

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
            
            while (m_ReflectionData.size() < set)
                m_ReflectionData.emplace_back();
            m_ReflectionData[set].emplace_back(resource.id, ShaderResourceType::Texture, accessType, binding, set, imageType.array.empty() ? 1 : imageType.array[0]);

			FL_LOG_DEBUG("    Image (%s)", resource.name.c_str());
            FL_LOG_DEBUG("      ArrayCount = %d", imageType.array[0]);
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
            
            while (m_ReflectionData.size() < set)
                m_ReflectionData.emplace_back();
            m_ReflectionData[set].emplace_back(resource.id, ShaderResourceType::StorageTexture, accessType, binding, set, imageType.array.empty() ? 1 : imageType.array[0]);

			FL_LOG_DEBUG("    StorageImage (%s)", resource.name.c_str());
            FL_LOG_DEBUG("      ArrayCount = %d", imageType.array[0]);
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
            
            while (m_ReflectionData.size() < set)
                m_ReflectionData.emplace_back();
            m_ReflectionData[set].emplace_back(resource.id, ShaderResourceType::SubpassInput, accessType, binding, set, 1);

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

        // Ensure there are no duplicate binding indices within sets and sort each set
        for (auto& set : m_ReflectionData)
        {
            if (set.empty())
                continue;

            #ifdef FL_DEBUG
            for (auto& check : set)
                for (auto& elem : set)
                    if (check.BindingIndex == elem.BindingIndex)
                        FL_ASSERT(check.UniqueId != elem.UniqueId, "Binding index must be unique for all shader resources");
            #endif

            std::sort(set.begin(), set.end(), [](const ReflectionDataElement& a, const ReflectionDataElement& b)
            {
                return a.BindingIndex < b.BindingIndex;
            });
        }
    }
}
