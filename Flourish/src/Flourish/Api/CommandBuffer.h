#pragma once

namespace Flourish
{
    enum GPUWorkloadType
    {
        Graphics = 0,
        Transfer,
        Compute
    };

    struct CommandBufferCreateInfo
    {
        GPUWorkloadType WorkloadType;
    };

    class CommandBuffer
    {
    public:
        virtual ~CommandBuffer() = default;

    public:
        // TS
        static std::shared_ptr<CommandBuffer> Create(const CommandBufferCreateInfo& createInfo);

    protected:
        GPUWorkloadType m_WorkloadType;
    };
}