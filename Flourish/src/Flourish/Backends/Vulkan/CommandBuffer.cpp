#include "flpch.h"
#include "CommandBuffer.h"

#include "Flourish/Backends/Vulkan/Context.h"
#include "Flourish/Api/RenderCommandEncoder.h"
#include "Flourish/Backends/Vulkan/Framebuffer.h"
#include "Flourish/Backends/Vulkan/Util/Synchronization.h"

namespace Flourish::Vulkan
{
    CommandBuffer::CommandBuffer(const CommandBufferCreateInfo& createInfo)
        : Flourish::CommandBuffer(createInfo)
    {
        m_GraphicsCommandEncoder = GraphicsCommandEncoder(this, m_Info.FrameRestricted);
        m_RenderCommandEncoder = RenderCommandEncoder(this, m_Info.FrameRestricted);
        m_ComputeCommandEncoder = ComputeCommandEncoder(this, m_Info.FrameRestricted);
        m_TransferCommandEncoder = TransferCommandEncoder(this, m_Info.FrameRestricted);
    }

    CommandBuffer::~CommandBuffer()
    {

    }
    
    void CommandBuffer::SubmitEncodedCommands(const CommandBufferEncoderSubmission& submission)
    {
        m_EncoderSubmissions.emplace_back(submission);
        m_Encoding = false;
    }

    Flourish::GraphicsCommandEncoder* CommandBuffer::EncodeGraphicsCommands()
    {
        CheckFrameUpdate();

        FL_CRASH_ASSERT(!m_Encoding, "Cannot begin encoding while another encoding is in progress");
        m_Encoding = true;

        m_GraphicsCommandEncoder.BeginEncoding();

        return static_cast<Flourish::GraphicsCommandEncoder*>(&m_GraphicsCommandEncoder);
    }

    Flourish::RenderCommandEncoder* CommandBuffer::EncodeRenderCommands(Flourish::Framebuffer* framebuffer)
    {
        CheckFrameUpdate();

        FL_CRASH_ASSERT(!m_Encoding, "Cannot begin encoding while another encoding is in progress");
        m_Encoding = true;
        
        m_RenderCommandEncoder.BeginEncoding(
            static_cast<Framebuffer*>(framebuffer)
        );

        return static_cast<Flourish::RenderCommandEncoder*>(&m_RenderCommandEncoder);
    }

    Flourish::ComputeCommandEncoder* CommandBuffer::EncodeComputeCommands()
    {
        CheckFrameUpdate();

        FL_CRASH_ASSERT(!m_Encoding, "Cannot begin encoding while another encoding is in progress");
        m_Encoding = true;

        m_ComputeCommandEncoder.BeginEncoding();

        return static_cast<Flourish::ComputeCommandEncoder*>(&m_ComputeCommandEncoder);
    }

    Flourish::TransferCommandEncoder* CommandBuffer::EncodeTransferCommands()
    {
        CheckFrameUpdate();

        FL_CRASH_ASSERT(!m_Encoding, "Cannot begin encoding while another encoding is in progress");
        m_Encoding = true;

        m_TransferCommandEncoder.BeginEncoding();

        return static_cast<Flourish::TransferCommandEncoder*>(&m_TransferCommandEncoder);
    }
    
    void CommandBuffer::CheckFrameUpdate()
    {
        if (!m_Info.FrameRestricted) return;
            
        // Each new frame, we need to clear the previous encoder submissions
        if (m_LastFrameEncoding != Flourish::Context::FrameCount())
        {
            m_LastFrameEncoding = Flourish::Context::FrameCount();
            
            ClearSubmissions();
        }
    }
}
