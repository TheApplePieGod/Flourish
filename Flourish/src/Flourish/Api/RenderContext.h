#pragma once

#include "Flourish/Api/Context.h"

class GLFWwindow;
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

        virtual void Present() = 0;
        virtual void UpdateDimensions(u32 width, u32 height) = 0;
        virtual RenderPass* GetRenderPass() const = 0;
        virtual bool Validate() = 0;
        [[nodiscard]] virtual RenderCommandEncoder* EncodeRenderCommands() = 0;

        inline void AddDependency(CommandBuffer* buffer) { m_Dependencies.push_back(buffer); }
        inline void ClearDependencies() { m_Dependencies.clear(); }

    public:
        // TS
        static std::shared_ptr<RenderContext> Create(const RenderContextCreateInfo& createInfo);

    protected:
        std::vector<CommandBuffer*> m_Dependencies;
    };
}
