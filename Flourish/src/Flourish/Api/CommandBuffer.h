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
        
        [[nodiscard]] virtual GraphicsCommandEncoder* EncodeGraphicsCommands() = 0;
        [[nodiscard]] virtual RenderCommandEncoder* EncodeRenderCommands(Framebuffer* framebuffer) = 0;
        [[nodiscard]] virtual ComputeCommandEncoder* EncodeComputeCommands() = 0;
        [[nodiscard]] virtual TransferCommandEncoder* EncodeTransferCommands() = 0;

        inline void AddDependency(CommandBuffer* buffer) { m_Dependencies.push_back(buffer); }
        inline void ClearDependencies() { m_Dependencies.clear(); }

        // TS
        inline bool IsEncoding() const { return m_Encoding; }
        inline bool IsFrameRestricted() const { return m_Info.FrameRestricted; }
        inline const auto& GetDependencies() const { return m_Dependencies; }

    public:
        // TS
        static std::shared_ptr<CommandBuffer> Create(const CommandBufferCreateInfo& createInfo);

    protected:
        CommandBufferCreateInfo m_Info;
        bool m_Encoding = false;
        std::vector<CommandBuffer*> m_Dependencies;
    };
}
