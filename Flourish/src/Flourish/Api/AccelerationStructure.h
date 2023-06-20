#pragma once

namespace Flourish
{
    enum class AccelerationStructureType
    {
        Node = 0,
        Scene
    };

    struct AccelerationStructureCreateInfo
    {
        AccelerationStructureType Type;
    };

    // TODO: Rename class?
    class Buffer;
    class AccelerationStructure
    {
    public:
        AccelerationStructure(const AccelerationStructureCreateInfo& createInfo)
            : m_Info(createInfo)
        {}
        virtual ~AccelerationStructure() = default;

        virtual void Build(void* vertexData, u32 vertexStride, u32 vertexCount, u32* indexData, u32 indexCount) = 0;
        virtual void Build(Buffer* vertexBuffer, Buffer* indexBuffer) = 0;

    public:
        static std::shared_ptr<AccelerationStructure> Create(const AccelerationStructureCreateInfo& createInfo);

    protected:
        AccelerationStructureCreateInfo m_Info;
    };
}
