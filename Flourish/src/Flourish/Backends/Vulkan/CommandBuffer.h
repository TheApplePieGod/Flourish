#pragma once

#include "Flourish/Api/CommandBuffer.h"
#include "Flourish/Backends/Vulkan/Util/Common.h"

namespace Flourish::Vulkan
{
    class CommandBuffer : public Flourish::CommandBuffer
    {
    public:
        CommandBuffer(const CommandBufferCreateInfo& createInfo, bool isPrimary = true);
        ~CommandBuffer() override;

        void BeginRecording() override;
        void BeginRecording(const VkCommandBufferInheritanceInfo& inheritanceInfo);
        void EndRecording() override;

        // TS
        void ExecuteRenderCommands(Flourish::RenderCommandEncoder* encoder) override;
        
        // TS
        VkCommandBuffer GetCommandBuffer() const;

    private:
        std::array<VkCommandBuffer, Flourish::Context::MaxFrameBufferCount> m_CommandBuffers;
        std::thread::id m_AllocatedThread;
    };
}