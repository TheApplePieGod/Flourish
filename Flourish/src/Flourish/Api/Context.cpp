#include "flpch.h"
#include "Context.h"

#include "Flourish/Backends/Vulkan/Util/Context.h"

namespace Flourish
{
    void Context::Initialize(const ContextInitializeInfo& initInfo)
    {
        if (s_BackendType != BackendType::None)
        {
            FL_ASSERT(false, "Cannot initialize, context has already been initialized");
            return;
        }

        s_FrameBufferCount = initInfo.FrameBufferCount;
        if (s_FrameBufferCount > MaxFrameBufferCount)
        {
            FL_LOG_WARN("Frame buffer count is limited to %d", MaxFrameBufferCount);
            s_FrameBufferCount = MaxFrameBufferCount;
        }

        s_BackendType = initInfo.Backend;
        switch (s_BackendType)
        {
            case BackendType::Vulkan: { Vulkan::Context::Initialize(initInfo); } return;
        }

        FL_ASSERT(false, "Context initialization is missing for selected api type");
    }

    void Context::Shutdown()
    {
        if (s_BackendType == BackendType::None)
        {
            FL_ASSERT(false, "Cannot shutdown, context has not been initialized");
            return;
        }

        switch (s_BackendType)
        {
            case BackendType::Vulkan: { Vulkan::Context::Shutdown(); } return;
        }

        FL_ASSERT(false, "Context shutdown is missing for selected api type");
    }
}