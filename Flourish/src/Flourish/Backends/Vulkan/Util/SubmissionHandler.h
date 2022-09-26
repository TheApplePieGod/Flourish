#pragma once

#include "Flourish/Backends/Vulkan/Util/Common.h"

namespace Flourish::Vulkan
{
    class RenderContext;
    class SubmissionHandler
    {
    public:
        void Initialize();
        void Shutdown();

        void ProcessSubmissions();

        // TS
        void PresentRenderContext(const RenderContext* context);
        
    private:
        VkSemaphore GetSemaphore();

    private:
        u32 m_SemaphorePoolIndex = 0;
        std::array<std::vector<VkSemaphore>, Flourish::Context::MaxFrameBufferCount> m_SemaphorePools;
        std::vector<const RenderContext*> m_PresentingContexts;
        std::mutex m_PresentingContextsLock;
    };
}