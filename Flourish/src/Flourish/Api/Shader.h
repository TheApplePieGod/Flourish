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
            Compute = 1 << 2,
            RayGen = 1 << 3,
            RayMiss = 1 << 4,
            RayIntersection = 1 << 5,
            RayClosestHit = 1 << 6,
            RayAnyHit = 1 << 7,
            All = 255
        };
    }
    typedef ShaderTypeEnum::Value ShaderTypeFlags;
    typedef u8 ShaderType;

    enum class ShaderResourceType
    {
        None = 0,
        UniformBuffer,
        StorageBuffer,
        Texture,
        StorageTexture,
        SubpassInput,
        AccelerationStructure
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
            : m_Info(createInfo)
        {}
        virtual ~Shader() = default;

        virtual bool Reload() = 0;

        // TS
        inline ShaderType GetType() const { return m_Info.Type; }

    public:
        // TS
        static std::shared_ptr<Shader> Create(const ShaderCreateInfo& createInfo);

    protected:
        ShaderCreateInfo m_Info;
    };
}
