#pragma once

#include "Flourish/Backends/Vulkan/RenderGraph.h"
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
        
    private:
        void PresentContext(RenderContext* context);
        void ProcessGraph(
            RenderGraph* graph,
            bool frameScope,
            std::function<void(VkSubmitInfo&, VkTimelineSemaphoreSubmitInfo&)>&& preSubmitCallback
        );
        void ProcessSubmission(
            Flourish::RenderGraph* const* graphs,
            u32 graphCount,
            bool frameScope,
            std::vector<VkFence>* finalFences = nullptr,
            std::vector<VkSemaphore>* finalSemaphores = nullptr,
            std::vector<u64>* finalSemaphoreValues = nullptr
        );
        void ProcessSingleSubmissionSequential(
            RenderGraph* graph,
            bool frameScope,
            std::vector<VkFence>* finalFences = nullptr,
            std::vector<VkSemaphore>* finalSemaphores = nullptr
        );
        void ProcessSingleSubmissionWithTimelines(
            RenderGraph* graph,
            bool frameScope,
            std::vector<VkFence>* finalFences = nullptr,
            std::vector<VkSemaphore>* finalSemaphores = nullptr,
            std::vector<u64>* finalSemaphoreValues = nullptr
        );

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
        std::array<std::vector<VkFence>, Flourish::Context::MaxFrameBufferCount> m_FrameWaitFences;
        std::vector<VkPipelineStageFlags> m_FrameWaitFlags;
        std::vector<VkPipelineStageFlags> m_RenderContextWaitFlags;
    };
}
