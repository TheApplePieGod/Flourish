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

    };

    class Framebuffer;
    class RenderCommandEncoder;
    class CommandBuffer
    {
    public:
        CommandBuffer(const CommandBufferCreateInfo& createInfo)
            : m_Info(createInfo)
        {}
        virtual ~CommandBuffer() = default;
        
        virtual RenderCommandEncoder* EncodeRenderCommands(Framebuffer* framebuffer) = 0;

        // TS
        inline bool IsEncoding() const { return m_Encoding; }

    public:
        // TS
        static std::shared_ptr<CommandBuffer> Create(const CommandBufferCreateInfo& createInfo);

    protected:
        CommandBufferCreateInfo m_Info;
        bool m_Encoding = false;
    };
}