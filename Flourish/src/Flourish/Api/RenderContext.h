#pragma once

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

    class RenderContext
    {
    public:
        virtual ~RenderContext() = default;

    public:
        // TS
        static std::shared_ptr<RenderContext> Create(const RenderContextCreateInfo& createInfo);
    };
}