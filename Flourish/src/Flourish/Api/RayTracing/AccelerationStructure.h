#pragma once

namespace Flourish
{
    enum class AccelerationStructureType
    {
        Node = 0,
        Scene
    };

    class AccelerationStructure;
    struct AccelerationStructureInstance
    {
        float* TransformMatrix; // 4x4 column-major (64 bytes)
        const AccelerationStructure* Parent;
    };

    struct AccelerationStructureCreateInfo
    {
        AccelerationStructureType Type;
        bool AllowUpdating = false;
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

        virtual void BuildNode(
            Buffer* vertexBuffer,
            Buffer* indexBuffer,
            bool update = false
        ) = 0;
        virtual void BuildScene(
            AccelerationStructureInstance* instances,
            u32 instanceCount,
            bool update = false
        ) = 0;

    public:
        static std::shared_ptr<AccelerationStructure> Create(const AccelerationStructureCreateInfo& createInfo);

    protected:
        AccelerationStructureCreateInfo m_Info;
    };
}
