#pragma once

namespace Flourish
{
    namespace ShaderTypeEnum
    {
        enum Value : u8
        {
            None = 0,
            Vertex = 1 << 0,
            Fragment = 1 << 1,
            Compute = 1 << 2
        };
    }
    typedef ShaderTypeEnum::Value ShaderTypeFlags;
    typedef u8 ShaderType;

    enum class ShaderResourceType
    {
        None = 0,
        UniformBuffer, StorageBuffer, Texture, StorageTexture, SubpassInput
    };

    struct ShaderCreateInfo
    {
        ShaderType Type;
        std::string_view Source;
        std::string_view Path;
    };

    class Shader
    {        
    public:
        Shader(const ShaderCreateInfo& createInfo)
            : m_Type(createInfo.Type)
        {}
        virtual ~Shader() = default;

    public:
        // TS
        static std::shared_ptr<Shader> Create(const ShaderCreateInfo& createInfo);

    protected:
        ShaderType m_Type;
    };
}
