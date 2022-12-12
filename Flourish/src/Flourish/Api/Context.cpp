#include "flpch.h"
#include "Context.h"

#include "Flourish/Backends/Vulkan/Context.h"

namespace Flourish
{
    void ContextCommandSubmissions::Clear()
    {
        Buffers.clear();
        Counts.clear();
        Contexts.clear();
    }

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

        s_FrameSubmissions.Clear();
        s_FrameCount++;
        s_FrameIndex = (s_FrameIndex + 1) % FrameBufferCount();
    }
    
    void Context::PushFrameCommandBuffers(const std::vector<std::vector<CommandBuffer*>>& buffers)
    {
        if (buffers.empty()) return;
        
        #if defined(FL_DEBUG) && defined(FL_ENABLE_ASSERTS)
            for (auto& list : buffers)
                for (auto buffer : list)
                    FL_ASSERT(buffer->IsFrameRestricted(), "Cannot include a non frame restricted command buffer in PushFrameCommandBuffers");
        #endif

        s_FrameSubmissions.Mutex.lock();
        s_FrameSubmissions.Buffers.insert(s_FrameSubmissions.Buffers.end(), buffers.begin(), buffers.end());
        s_FrameSubmissions.Counts.push_back(buffers.size());
        s_FrameSubmissions.Mutex.unlock();
    }

    void Context::PushCommandBuffers(const std::vector<std::vector<CommandBuffer*>>& buffers, std::function<void()> callback)
    {
        if (buffers.empty()) return;
        
        #if defined(FL_DEBUG) && defined(FL_ENABLE_ASSERTS)
            for (auto& list : buffers)
                for (auto buffer : list)
                    FL_ASSERT(!buffer->IsFrameRestricted(), "Cannot include a frame restricted command buffer in PushCommandBuffers");
        #endif

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

    void Context::PushFrameRenderContext(RenderContext* context)
    {
        s_FrameSubmissions.Mutex.lock();
        s_FrameSubmissions.Contexts.push_back(context);
        s_FrameSubmissions.Mutex.unlock();
    }
}