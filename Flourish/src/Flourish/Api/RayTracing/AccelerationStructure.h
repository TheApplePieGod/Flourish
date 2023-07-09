#pragma once

namespace Flourish
{
    namespace AccelerationStructureInstanceSettingsEnum
    {
        enum Value : u8
        {
            ForceOpaque = (1 << 0),
            ForceNoOpaque = (1 << 1)
        };
    }
    typedef AccelerationStructureInstanceSettingsEnum::Value AccelerationStructureInstanceSettingsFlags;
    typedef u8 AccelerationStructureInstanceSettings;

    enum class AccelerationStructureType
    {
        Node = 0,
        Scene
    };

    enum class AccelerationStructureBuildFrequency
    {
        Once = 0,
        Rarely,
        Often
    };

    enum class AccelerationStructurePerformanceType
    {
        FasterRuntime = 0,
        FasterBuilds
    };

    class AccelerationStructure;
    struct AccelerationStructureInstance
    {
        const float* TransformMatrix; // 4x4 column-major (64 bytes)
        const AccelerationStructure* Parent;
        u32 CustomIndex = 0;
        AccelerationStructureInstanceSettings Settings = 0;
    };

    class Buffer;
    struct AccelerationStructureNodeBuildInfo
    {
        Buffer* VertexBuffer = nullptr;
        Buffer* IndexBuffer = nullptr;

        Buffer* AABBBuffer = nullptr;

        bool Opaque = true;
        bool TryUpdate = false;
        bool AsyncCompletion = false;
        std::function<void()> CompletionCallback = nullptr;
    };

    struct AccelerationStructureSceneBuildInfo
    {
        AccelerationStructureInstance* Instances;
        u32 InstanceCount;
        bool TryUpdate = false;
        bool AsyncCompletion = false;
        std::function<void()> CompletionCallback = nullptr;
    };

    struct AccelerationStructureCreateInfo
    {
        AccelerationStructureType Type;
        AccelerationStructurePerformanceType PerformancePreference = AccelerationStructurePerformanceType::FasterRuntime;
        AccelerationStructureBuildFrequency BuildFrequency = AccelerationStructureBuildFrequency::Once;
        bool AllowUpdating = false;
    };

    // TODO: Rename class?
    class AccelerationStructure
    {
    public:
        AccelerationStructure(const AccelerationStructureCreateInfo& createInfo)
            : m_Info(createInfo)
        {}
        virtual ~AccelerationStructure() = default;

        virtual void RebuildNode(const AccelerationStructureNodeBuildInfo& buildInfo) = 0;
        virtual void RebuildScene(const AccelerationStructureSceneBuildInfo& buildInfo) = 0;

        // Whether the resource is allowed to be used on the CPU, not if it has
        // completed building on the GPU
        virtual bool IsBuilt() const = 0;

    public:
        static std::shared_ptr<AccelerationStructure> Create(const AccelerationStructureCreateInfo& createInfo);

    protected:
        AccelerationStructureCreateInfo m_Info;
    };
}
