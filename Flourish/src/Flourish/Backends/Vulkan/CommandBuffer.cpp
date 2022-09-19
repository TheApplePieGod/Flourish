#include "flpch.h"
#include "CommandBuffer.h"

#include "Flourish/Backends/Vulkan/Util/Context.h"
#include "Flourish/Api/RenderCommandEncoder.h"
#include "Flourish/Backends/Vulkan/RenderCommandEncoder.h"

namespace Flourish::Vulkan
{
    CommandBuffer::CommandBuffer(const CommandBufferCreateInfo& createInfo, bool isPrimary)
        : Flourish::CommandBuffer(createInfo)
    {
        m_AllocatedThread = std::this_thread::get_id();
        Context::Commands().AllocateBuffers(
            m_Info.WorkloadType,
            !isPrimary,
            m_CommandBuffers.data(),
            Flourish::Context::FrameBufferCount(),
            m_AllocatedThread
        );   
    }

    CommandBuffer::~CommandBuffer()
    {
        FL_ASSERT(
            m_AllocatedThread == std::this_thread::get_id(),
            "Command buffer should never be destroyed from a thread different than the one that created it"
        );
        FL_ASSERT(
            Flourish::Context::IsThreadRegistered(),
            "Destroying command buffer on deregistered thread, which will cause a memory leak"
        );

        auto buffers = m_CommandBuffers;
        auto thread = m_AllocatedThread;
        auto workloadType = m_Info.WorkloadType;
        Context::DeleteQueue().Push([=]()
        {
            Context::Commands().FreeBuffers(
                workloadType,
                buffers.data(),
                Flourish::Context::FrameBufferCount(),
                thread
            );
        });
    }
    
    void CommandBuffer::BeginRecording()
    {
        VkCommandBufferInheritanceInfo inheritanceInfo{};
        inheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;

        BeginRecording(inheritanceInfo);
    }
    
    void CommandBuffer::BeginRecording(const VkCommandBufferInheritanceInfo& inheritanceInfo)
    {
        FL_CRASH_ASSERT(!m_Recording, "Cannot begin command buffer recording that has already begun");

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = m_Info.Reusable ? VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT : VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        beginInfo.pInheritanceInfo = &inheritanceInfo;

        // TODO: check result?
        VkCommandBuffer buffer = m_CommandBuffers[Flourish::Context::FrameIndex()];
        vkBeginCommandBuffer(buffer, &beginInfo);
        
        m_Recording = true;
    }

    void CommandBuffer::EndRecording()
    {
        FL_CRASH_ASSERT(m_Recording, "Cannot end command buffer recording that has not begun");
        
        VkCommandBuffer buffer = m_CommandBuffers[Flourish::Context::FrameIndex()];
        vkEndCommandBuffer(buffer);

        m_Recording = false;
    }

    void CommandBuffer::ExecuteRenderCommands(Flourish::RenderCommandEncoder* _encoder)
    {
        FL_CRASH_ASSERT(m_Recording, "Cannot record ExecuteRenderCommands before recording has begun");

        RenderCommandEncoder* encoder = static_cast<RenderCommandEncoder*>(_encoder);
        VkCommandBuffer buffer = encoder->GetCommandBuffer();

        m_RecordLock.lock(); 
        vkCmdBeginRenderPass(buffer, &encoder->GetRenderPassBeginInfo(), VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
        vkCmdExecuteCommands(GetCommandBuffer(), 1, &buffer);
        vkCmdEndRenderPass(buffer);
        m_RecordLock.unlock();
    }
    
    VkCommandBuffer CommandBuffer::GetCommandBuffer() const
    {
        return m_CommandBuffers[Flourish::Context::FrameIndex()];
    }
}