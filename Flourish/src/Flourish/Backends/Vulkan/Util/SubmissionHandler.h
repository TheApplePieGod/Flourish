#pragma once

#include "Flourish/Api/RenderGraph.h"
#include "Flourish/Backends/Vulkan/Util/Common.h"
#include "Flourish/Backends/Vulkan/Util/Synchronization.h"

namespace Flourish::Vulkan
{
    class Framebuffer;
    class RenderContext;
    class SubmissionHandler
    {
    public:
        void Initialize();
        void Shutdown();

        void WaitOnFrameSemaphores();
        void ProcessFrameSubmissions();
        
        // TS
        void ProcessPushSubmission(Flourish::RenderGraph* graph, std::function<void()> callback = nullptr);
        void ProcessExecuteSubmission(Flourish::RenderGraph* graph);
        
    public:
        static void ProcessSubmission(
            Flourish::RenderGraph* const* graphs,
            u32 graphCount,
            std::vector<VkSemaphore>* finalSemaphores = nullptr,
            std::vector<u64>* finalSemaphoreValues = nullptr
        );

    private:
        void PresentContext(RenderContext* context, u32 waitSemaphoreCount);

    private:
        static void ExecuteRenderPassCommands(
            VkCommandBuffer primary,
            Framebuffer* framebuffer,
            const VkCommandBuffer* subpasses,
            u32 subpassCount
        );
        
    private:
        std::array<std::vector<VkSemaphore>, Flourish::Context::MaxFrameBufferCount> m_FrameWaitSemaphores;
        std::array<std::vector<u64>, Flourish::Context::MaxFrameBufferCount> m_FrameWaitSemaphoreValues;
        std::vector<VkPipelineStageFlags> m_RenderContextWaitFlags;
    };
}
