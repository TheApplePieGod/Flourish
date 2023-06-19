#pragma once

namespace Flourish
{
    enum class GPUWorkloadType : u32
    {
        Graphics = 0,
        Transfer = 1,
        Compute = 2
    };

    struct CommandBufferCreateInfo
    {
        bool FrameRestricted = true;
        std::string DebugName;
    };

    class Framebuffer;
    class GraphicsCommandEncoder;
    class RenderCommandEncoder;
    class ComputeCommandEncoder;
    class TransferCommandEncoder;
    class CommandBuffer
    {
    public:
        CommandBuffer(const CommandBufferCreateInfo& createInfo);
        virtual ~CommandBuffer() = default;
        
        [[nodiscard]] virtual GraphicsCommandEncoder* EncodeGraphicsCommands() = 0;
        [[nodiscard]] virtual RenderCommandEncoder* EncodeRenderCommands(Framebuffer* framebuffer) = 0;
        [[nodiscard]] virtual ComputeCommandEncoder* EncodeComputeCommands() = 0;
        [[nodiscard]] virtual TransferCommandEncoder* EncodeTransferCommands() = 0;

        // TS
        inline u64 GetId() const { return m_Id; }
        inline bool IsEncoding() const { return m_Encoding; }
        inline bool IsFrameRestricted() const { return m_Info.FrameRestricted; }
        inline std::string_view GetDebugName() const { return m_Info.DebugName; }

    public:
        // TS
        static std::shared_ptr<CommandBuffer> Create(const CommandBufferCreateInfo& createInfo);

    protected:
        CommandBufferCreateInfo m_Info;
        bool m_Encoding = false;
        u64 m_Id;
    };
}
