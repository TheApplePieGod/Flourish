#pragma once

#include "Flourish/Api/Context.h"
#include "Flourish/Core/PlatformDetection.h"

class GLFWwindow;
namespace Flourish
{
    struct RenderContextCreateInfo
    {
        #ifdef FL_USE_GLFW
        GLFWwindow* Window;        
        #elif defined(FL_PLATFORM_ANDROID)
        ANativeWindow* Window;
        #elif defined(FL_PLATFORM_WINDOWS)
        HINSTANCE Instance;
        HWND Window;
        #elif defined(FL_PLATFORM_LINUX)
        xcb_connection_t* Connection;
        xcb_window_t Window;
        #elif defined(FL_PLATFORM_MACOS)
        void* CAMetalLayer;
        #endif
        u32 Width;
        u32 Height;
        std::array<float, 4> ClearColor = { 0.f, 0.f, 0.f, 0.f };
    };

    class RenderCommandEncoder;
    class Framebuffer;
    class RenderPass;
    class RenderContext
    {
    public:
        RenderContext(const RenderContextCreateInfo& createInfo)
        {}
        virtual ~RenderContext() = default;

        virtual void UpdateDimensions(u32 width, u32 height) = 0;
        virtual RenderPass* GetRenderPass() const = 0;
        virtual bool Validate() = 0;

        // Can only encode once per frame
        [[nodiscard]] virtual RenderCommandEncoder* EncodeRenderCommands() = 0;

    public:
        // TS
        static std::shared_ptr<RenderContext> Create(const RenderContextCreateInfo& createInfo);
    };
}
