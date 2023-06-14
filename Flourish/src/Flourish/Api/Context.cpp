#include "flpch.h"
#include "Context.h"

#include "Flourish/Backends/Vulkan/Context.h"

namespace Flourish
{

    void Context::Initialize(const ContextInitializeInfo& initInfo)
    {
        FL_ASSERT(s_BackendType == BackendType::None, "Cannot initialize, context has already been initialized");

        s_ReversedZBuffer = initInfo.UseReversedZBuffer;
        s_FrameBufferCount = initInfo.FrameBufferCount;
        if (s_FrameBufferCount > MaxFrameBufferCount)
        {
            FL_LOG_WARN("Frame buffer count is limited to %d", MaxFrameBufferCount);
            s_FrameBufferCount = MaxFrameBufferCount;
        }

        s_BackendType = initInfo.Backend;
        switch (s_BackendType)
        {
            default: { FL_ASSERT(false, "Context initialization is missing for selected api type"); } return;
            case BackendType::Vulkan: { Vulkan::Context::Initialize(initInfo); } break;
        }
    }

    void Context::Shutdown(std::function<void()> finalizer)
    {
        FL_ASSERT(s_BackendType != BackendType::None, "Cannot shutdown, context has not been initialized");

        switch (s_BackendType)
        {
            case BackendType::Vulkan: { Vulkan::Context::Shutdown(finalizer); } return;
        }

        FL_ASSERT(false, "Context shutdown is missing for selected api type");
    }

    void Context::BeginFrame()
    {
        FL_ASSERT(s_BackendType != BackendType::None, "Cannot begin frame, context has not been initialized");

        switch (s_BackendType)
        {
            case BackendType::Vulkan: { Vulkan::Context::BeginFrame(); } break;
        }
    }
    
    void Context::EndFrame()
    {
        FL_ASSERT(s_BackendType != BackendType::None, "Cannot end frame, context has not been initialized");

        switch (s_BackendType)
        {
            case BackendType::Vulkan: { Vulkan::Context::EndFrame(); } break;
        }

        s_FrameSubmissions.clear();
        s_FrameCount++;
        s_FrameIndex = (s_FrameIndex + 1) % FrameBufferCount();
    }
    
    void Context::PushFrameRenderGraph(RenderGraph* graph)
    {
        if (!graph) return;
        
        s_FrameMutex.lock();
        s_FrameSubmissions.emplace_back(graph);
        s_FrameMutex.unlock();

        switch (s_BackendType)
        {
            //case BackendType::Vulkan: { Vulkan::Context::SubmissionHandler().ProcessFrameSubmissions(buffers, false); } break;
            //case BackendType::Vulkan: { Vulkan::Context::SubmissionHandler().ProcessFrameSubmissions2(buffers, bufferCount, false); } break;
        }
    }

    void Context::PushCommandBuffers(const std::vector<std::vector<CommandBuffer*>>& buffers, std::function<void()> callback)
    {
        if (buffers.empty()) return;
        
        switch (s_BackendType)
        {
            case BackendType::Vulkan: { Vulkan::Context::SubmissionHandler().ProcessPushSubmission(buffers, callback); } break;
        }
    }

    void Context::ExecuteCommandBuffers(const std::vector<std::vector<CommandBuffer*>>& buffers)
    {
        if (buffers.empty()) return;
        
        #if defined(FL_DEBUG) && defined(FL_ENABLE_ASSERTS)
            for (auto& list : buffers)
                for (auto buffer : list)
                    FL_ASSERT(!buffer->IsFrameRestricted(), "Cannot include a frame restricted command buffer in ExecuteCommandBuffers");
        #endif

        switch (s_BackendType)
        {
            case BackendType::Vulkan: { Vulkan::Context::SubmissionHandler().ProcessExecuteSubmission(buffers); } break;
        }
    }
}
