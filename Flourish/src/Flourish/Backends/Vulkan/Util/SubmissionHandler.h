#pragma once

#include "Flourish/Api/RenderGraph.h"
#include "Flourish/Backends/Vulkan/Util/Common.h"
#include "Flourish/Backends/Vulkan/Util/Synchronization.h"

namespace Flourish::Vulkan
{
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
            u32 frameIndex,
            std::vector<VkSemaphore>* finalSemaphores = nullptr,
            std::vector<u64>* finalSemaphoreValues = nullptr
        );
        
    private:
        std::array<std::vector<VkSemaphore>, Flourish::Context::MaxFrameBufferCount> m_FrameWaitSemaphores;
        std::array<std::vector<u64>, Flourish::Context::MaxFrameBufferCount> m_FrameWaitSemaphoreValues;
        std::vector<VkPipelineStageFlags> m_RenderContextWaitFlags;
    };
}
