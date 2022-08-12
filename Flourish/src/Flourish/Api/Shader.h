#pragma once

namespace Flourish
{
    enum class ShaderResourceType
    {
        None = 0,
        UniformBuffer, StorageBuffer, Texture, SubpassInput
    };

    enum class ShaderResourceAccessType
    {
        None = 0,
        Vertex, Fragment, Both, Compute
    };

    struct ReflectionDataElement
    {
        ReflectionDataElement() = default;
        ReflectionDataElement(u32 uniqueId, ShaderResourceType resourceType, ShaderResourceAccessType accessType, u32 bindingIndex, u32 setIndex, u32 arrayCount)
            : UniqueId(uniqueId), ResourceType(resourceType), AccessType(accessType), BindingIndex(bindingIndex), SetIndex(setIndex), ArrayCount(arrayCount)
        {}

        u32 UniqueId;
        ShaderResourceType ResourceType;
        ShaderResourceAccessType AccessType;
        u32 BindingIndex;
        u32 SetIndex;
        u32 ArrayCount;
    };

    enum class ShaderType
    {
        None = 0,
        Vertex,
        Fragment,
        Compute
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

        // TS
        inline const auto& GetReflectionData() { return m_ReflectionData; }

    public:
        // TS
        static std::shared_ptr<Shader> Create(const ShaderCreateInfo& createInfo);

    protected:
        ShaderType m_Type;
        std::vector<ReflectionDataElement> m_ReflectionData;
    };
}