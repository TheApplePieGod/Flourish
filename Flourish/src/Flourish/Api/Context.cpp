#include "flpch.h"
#include "Context.h"

#include "Flourish/Backends/Vulkan/Util/Context.h"

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

    void Context::Shutdown()
    {
        FL_ASSERT(s_BackendType != BackendType::None, "Cannot shutdown, context has not been initialized");

        switch (s_BackendType)
        {
            case BackendType::Vulkan: { Vulkan::Context::Shutdown(); } return;
        }

        FL_ASSERT(false, "Context shutdown is missing for selected api type");
    }

    void Context::BeginFrame()
    {
        FL_ASSERT(s_BackendType != BackendType::None, "Cannot begin frame, context has not been initialized");

        s_SubmittedCommandBuffers.clear();
        s_SubmittedCommandBufferCounts.clear();
        s_FrameCount++;
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

        s_FrameIndex = (s_FrameIndex + 1) % FrameBufferCount();
    }
    
    int Context::SubmitCommandBuffers(const std::vector<std::vector<CommandBuffer*>>& buffers)
    {
        if (buffers.empty()) return -1;

        s_SubmittedCommandBuffersLock.lock();
        int submissionId = s_SubmittedCommandBufferCounts.size();
        s_SubmittedCommandBuffers.insert(s_SubmittedCommandBuffers.end(), buffers.begin(), buffers.end());
        s_SubmittedCommandBufferCounts.push_back(buffers.size());
        s_SubmittedCommandBuffersLock.unlock();

        return submissionId;
    }
}