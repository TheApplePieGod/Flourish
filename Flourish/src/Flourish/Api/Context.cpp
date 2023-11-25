#include "flpch.h"
#include "Context.h"

#include "Flourish/Backends/Vulkan/Context.h"

namespace Flourish
{
    unsigned char* ReadFileImpl(std::string_view path, u32& outLength)
    {
        std::ifstream file(path, std::ios::ate | std::ios::binary);
        if (!file.is_open())
            return nullptr;
        
        u32 fileSize = static_cast<u32>(file.tellg());
        unsigned char* buffer = new unsigned char[fileSize + 1];
        file.seekg(0, std::ios::beg);
        file.read((char*)buffer, fileSize);
        file.close();

        buffer[fileSize] = 0;

        outLength = fileSize;
        return buffer;
    }

    void Context::Initialize(const ContextInitializeInfo& initInfo)
    {
        FL_ASSERT(s_BackendType == BackendType::None, "Cannot initialize, context has already been initialized");

        s_ReversedZBuffer = initInfo.UseReversedZBuffer;
        s_FrameBufferCount = initInfo.FrameBufferCount;
        s_LastFrameIndex = s_FrameBufferCount - 1;
        if (s_FrameBufferCount > MaxFrameBufferCount)
        {
            FL_LOG_WARN("Frame buffer count is limited to %d", MaxFrameBufferCount);
            s_FrameBufferCount = MaxFrameBufferCount;
        }

        s_ReadFile = initInfo.ReadFile;
        if (!s_ReadFile)
            s_ReadFile = ReadFileImpl;

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
        FL_PROFILE_FUNCTION();

        FL_ASSERT(s_BackendType != BackendType::None, "Cannot begin frame, context has not been initialized");

        switch (s_BackendType)
        {
            case BackendType::Vulkan: { Vulkan::Context::BeginFrame(); } break;
        }
    }
    
    void Context::EndFrame()
    {
        FL_PROFILE_FUNCTION();

        FL_ASSERT(s_BackendType != BackendType::None, "Cannot end frame, context has not been initialized");

        switch (s_BackendType)
        {
            case BackendType::Vulkan: { Vulkan::Context::EndFrame(); } break;
        }

        s_GraphSubmissions.clear();
        s_ContextSubmissions.clear();
        s_FrameCount++;
        s_LastFrameIndex = s_FrameIndex;
        s_FrameIndex = (s_FrameIndex + 1) % FrameBufferCount();
    }
    
    void Context::PushFrameRenderGraph(RenderGraph* graph)
    {
        if (!graph) return;
        
        s_FrameMutex.lock();
        s_GraphSubmissions.emplace_back(graph);
        s_FrameMutex.unlock();

        switch (s_BackendType)
        {
            //case BackendType::Vulkan: { Vulkan::Context::SubmissionHandler().ProcessFrameSubmissions(buffers, false); } break;
            //case BackendType::Vulkan: { Vulkan::Context::SubmissionHandler().ProcessFrameSubmissions2(buffers, bufferCount, false); } break;
        }
    }

    void Context::PushFrameRenderContext(RenderContext* context)
    {
        if (!context) return;
        
        s_FrameMutex.lock();
        s_ContextSubmissions.emplace_back(context);
        s_FrameMutex.unlock();
    }

    void Context::PushRenderGraph(RenderGraph* graph, std::function<void()> callback)
    {
        if (!graph) return;
        
        switch (s_BackendType)
        {
            case BackendType::Vulkan: { Vulkan::Context::SubmissionHandler().ProcessPushSubmission(graph, callback); } break;
        }
    }

    void Context::ExecuteRenderGraph(RenderGraph* graph)
    {
        if (!graph) return;
        
        /*
        #if defined(FL_DEBUG) && defined(FL_ENABLE_ASSERTS)
            for (auto& list : buffers)
                for (auto buffer : list)
                    FL_ASSERT(!buffer->IsFrameRestricted(), "Cannot include a frame restricted command buffer in ExecuteCommandBuffers");
        #endif
        */

        switch (s_BackendType)
        {
            case BackendType::Vulkan: { Vulkan::Context::SubmissionHandler().ProcessExecuteSubmission(graph); } break;
        }
    }
}
