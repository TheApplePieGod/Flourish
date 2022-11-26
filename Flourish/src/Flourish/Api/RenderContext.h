#pragma once

#include "Flourish/Api/Context.h"

namespace Flourish
{
    struct RenderContextCreateInfo
    {
        #ifdef FL_USE_GLFW
        GLFWwindow* Window;        
        #elif defined(FL_PLATFORM_WINDOWS)
        HINSTANCE Instance;
        HWND Window;
        #elif defined(FL_PLATFORM_LINUX)
        xcb_connection_t* Connection;
        xcb_window_t Window;
        #else
        void* NSView;
        #endif
        u32 Width;
        u32 Height;
        std::array<float, 4> ClearColor = { 0.f, 0.f, 0.f, 0.f };
    };

    class RenderCommandEncoder;
    class Framebuffer;
    class CommandBuffer;
    class RenderPass;
    class RenderContext
    {
    public:
        RenderContext(const RenderContextCreateInfo& createInfo)
        {}
        virtual ~RenderContext() = default;

        virtual void Present(const std::vector<std::vector<CommandBuffer*>>& dependencyBuffers) = 0;
        virtual void UpdateDimensions(u32 width, u32 height) = 0;
        virtual RenderPass* GetRenderPass() const = 0;
        virtual bool Validate() = 0;
        [[nodiscard]] virtual RenderCommandEncoder* EncodeFrameRenderCommands() = 0;

    public:
        // TS
        static std::shared_ptr<RenderContext> Create(const RenderContextCreateInfo& createInfo);
    };
}