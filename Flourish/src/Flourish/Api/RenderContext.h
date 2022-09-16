#pragma once

#include "Flourish/Api/Context.h"

namespace Flourish
{
    struct RenderContextCreateInfo
    {
        #ifdef FL_PLATFORM_WINDOWS
        HINSTANCE Instance;
        HWND Window;
        #elif defined(FL_PLATFORM_LINUX)
        xcb_connection_t* Connection;
        xcb_window_t Window;
        #else

        #endif
        u32 Width;
        u32 Height;
    };

    class RenderCommandEncoder;
    class Framebuffer;
    class RenderContext
    {
    public:
        RenderContext(const RenderContextCreateInfo& createInfo)
        {}
        virtual ~RenderContext() = default;

        // TS
        virtual std::shared_ptr<RenderCommandEncoder> GetFrameRenderCommandEncoder() const; 
        
    public:
        // TS
        static std::shared_ptr<RenderContext> Create(const RenderContextCreateInfo& createInfo);
    };
}