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
        bool Reusable; // True if recorded commands might be submitted more than once
    };

    class RenderCommandEncoder;
    class CommandBuffer
    {
    public:
        CommandBuffer(const CommandBufferCreateInfo& createInfo)
            : m_Info(createInfo)
        {}
        virtual ~CommandBuffer() = default;
        
        virtual void BeginRecording() = 0;
        virtual void EndRecording() = 0;

        // TS
        virtual void ExecuteRenderCommands(RenderCommandEncoder* encoder) = 0;

        // TS
        inline bool IsRecording() const { return m_Recording; }

    public:
        // TS
        static std::shared_ptr<CommandBuffer> Create(const CommandBufferCreateInfo& createInfo);

    protected:
        CommandBufferCreateInfo m_Info;
        std::mutex m_RecordLock;
        bool m_Recording = false;
    };
}