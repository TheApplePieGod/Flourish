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
        u32 MaxEncoders = 8;
        bool FrameRestricted = true;
    };

    class Framebuffer;
    class ComputeTarget;
    class GraphicsCommandEncoder;
    class RenderCommandEncoder;
    class ComputeCommandEncoder;
    class TransferCommandEncoder;
    class CommandBuffer
    {
    public:
        CommandBuffer(const CommandBufferCreateInfo& createInfo)
            : m_Info(createInfo)
        {}
        virtual ~CommandBuffer() = default;
        
        virtual GraphicsCommandEncoder* EncodeGraphicsCommands() = 0;
        virtual RenderCommandEncoder* EncodeRenderCommands(Framebuffer* framebuffer) = 0;
        virtual ComputeCommandEncoder* EncodeComputeCommands(ComputeTarget* target) = 0;
        virtual TransferCommandEncoder* EncodeTransferCommands() = 0;

        // TS
        inline bool IsEncoding() const { return m_Encoding; }
        inline bool IsFrameRestricted() const { return m_Info.FrameRestricted; }

    public:
        // TS
        static std::shared_ptr<CommandBuffer> Create(const CommandBufferCreateInfo& createInfo);

    protected:
        CommandBufferCreateInfo m_Info;
        bool m_Encoding = false;
    };
}