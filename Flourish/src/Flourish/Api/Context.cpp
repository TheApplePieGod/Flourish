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

        // Register main thread
        RegisterThread();
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

    bool Context::IsThreadRegistered(std::thread::id thread)
    {
        s_RegisteredThreadsLock.lock();
        bool found = s_RegisteredThreads.find(thread) != s_RegisteredThreads.end();
        s_RegisteredThreadsLock.unlock();
        return found;
    }

    void Context::RegisterThread()
    {
        FL_ASSERT(s_BackendType != BackendType::None, "Cannot register thread, context has not been initialized");        

        auto thread = std::this_thread::get_id();
        s_RegisteredThreadsLock.lock();
        FL_ASSERT(s_RegisteredThreads.find(thread) == s_RegisteredThreads.end(), "Cannot register thread that has already been registered");
        s_RegisteredThreads.emplace(thread);
        s_RegisteredThreadsLock.unlock();

        switch (s_BackendType)
        {
            case BackendType::Vulkan: { Vulkan::Context::RegisterThread(); } return;
        }
    }

    void Context::UnregisterThread()
    {
        FL_ASSERT(s_BackendType != BackendType::None, "Cannot unregister thread, context has not been initialized");

        auto thread = std::this_thread::get_id();
        s_RegisteredThreadsLock.lock();
        FL_ASSERT(s_RegisteredThreads.find(thread) != s_RegisteredThreads.end(), "Cannot unregister thread that has not been registered");
        s_RegisteredThreads.erase(thread);
        s_RegisteredThreadsLock.unlock();

        switch (s_BackendType)
        {
            case BackendType::Vulkan: { Vulkan::Context::UnregisterThread(); } return;
        }
    }
}