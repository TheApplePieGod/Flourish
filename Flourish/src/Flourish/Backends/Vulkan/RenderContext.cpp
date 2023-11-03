#include "flpch.h"
#include "RenderContext.h"

#include "Flourish/Backends/Vulkan/Context.h"
#include "Flourish/Backends/Vulkan/Util/Synchronization.h"

#ifdef FL_USE_GLFW
#include "GLFW/glfw3.h"
#ifdef FL_PLATFORM_WINDOWS
#define GLFW_EXPOSE_NATIVE_WIN32
#include "GLFW/glfw3native.h"
#endif
#endif

namespace Flourish::Vulkan
{
    RenderContext::RenderContext(const RenderContextCreateInfo& createInfo)
        : Flourish::RenderContext(createInfo),
         m_CommandBuffer({})
    {
        auto instance = Context::Instance();

        // Create the surface
        void* windowHandle = nullptr;
        #ifdef FL_USE_GLFW
            auto result = glfwCreateWindowSurface(instance, createInfo.Window, nullptr, &m_Surface);
            #ifdef FL_PLATFORM_WINDOWS
                windowHandle = glfwGetWin32Window(createInfo.Window);
            #endif
        #elif defined(FL_PLATFORM_ANDROID)
	        VkAndroidSurfaceCreateInfoKHR surfaceInfo;
            surfaceInfo.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
            surfaceInfo.pNext = nullptr;
            surfaceInfo.flags = 0;
            surfaceInfo.window = createInfo.Window;

        	auto result = vkCreateAndroidSurfaceKHR(instance, &surfaceInfo, nullptr, &m_Surface);
        #elif defined(FL_PLATFORM_WINDOWS)
            VkWin32SurfaceCreateInfoKHR surfaceInfo{};
            surfaceInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
            surfaceInfo.pNext = nullptr;
            surfaceInfo.hinstance = createInfo.Instance;
            surfaceInfo.hwnd = createInfo.Window;

            windowHandle = createInfo.Window;

            auto result = vkCreateWin32SurfaceKHR(instance, &surfaceInfo, nullptr, &m_Surface);
        #elif defined(FL_PLATFORM_LINUX)
            VkXcbSurfaceCreateInfoKHR surfaceInfo{};
            surfaceInfo.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
            surfaceInfo.pNext = nullptr;
            surfaceInfo.connection = createInfo.connection;
            surfaceInfo.window = createInfo.window;

            auto result = vkCreateXcbSurfaceKHR(instance, &surfaceInfo, nullptr, &m_Surface);
        #else
            VkMetalSurfaceCreateInfoEXT surfaceInfo{};
            surfaceInfo.sType = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT;
            surfaceInfo.pLayer = createInfo.CAMetalLayer;

            auto result = vkCreateMetalSurfaceEXT(instance, &surfaceInfo, nullptr, &m_Surface);
        #endif

        if (!m_Surface)
        {
            FL_LOG_ERROR("RenderContext window surface failed to create with error %d", result);
            throw std::exception();
        }

        m_Swapchain.Initialize(createInfo, m_Surface, windowHandle);
        
        for (u32 frame = 0; frame < Flourish::Context::FrameBufferCount(); frame++)
        {
            m_SignalSemaphores[frame][0] = Synchronization::CreateTimelineSemaphore(0);
            m_SignalSemaphores[frame][1] = Synchronization::CreateSemaphore();
        }
    }

    RenderContext::~RenderContext()
    {
        m_Swapchain.Shutdown();

        auto surface = m_Surface;
        auto semaphores = m_SignalSemaphores;
        Context::FinalizerQueue().Push([=]()
        {
            if (surface)
                vkDestroySurfaceKHR(Context::Instance(), surface, nullptr);
            for (u32 frame = 0; frame < Flourish::Context::FrameBufferCount(); frame++)
            {
                vkDestroySemaphore(Context::Devices().Device(), semaphores[frame][0], nullptr);
                vkDestroySemaphore(Context::Devices().Device(), semaphores[frame][1], nullptr);
            }
        }, "Render context free");
    }

    void RenderContext::UpdateDimensions(u32 width, u32 height)
    {
        m_Swapchain.UpdateDimensions(width, height);
    }

    RenderPass* RenderContext::GetRenderPass() const
    {
        return m_Swapchain.GetRenderPass();
    }

    bool RenderContext::Validate()
    {
        FL_PROFILE_FUNCTION();

        if (!m_Swapchain.IsValid())
            m_Swapchain.RecreateImmediate();
        return m_Swapchain.IsValid();
    }

    Flourish::RenderCommandEncoder* RenderContext::EncodeRenderCommands()
    {
        FL_CRASH_ASSERT(m_Swapchain.IsValid(), "Cannot encode render commands on an invalid render context");

        if (m_LastEncodingFrame != Flourish::Context::FrameCount())
        {
            m_LastEncodingFrame = Flourish::Context::FrameCount();
            m_Swapchain.UpdateActiveImage();
            m_SignalValue++;
        }
        else
            throw std::exception();

        return m_CommandBuffer.EncodeRenderCommands(m_Swapchain.GetFramebuffer());
    } 

    VkSemaphore RenderContext::GetTimelineSignalSemaphore() const
    {
        return m_SignalSemaphores[Flourish::Context::FrameIndex()][0];
    }

    VkSemaphore RenderContext::GetBinarySignalSemaphore() const
    {
        return m_SignalSemaphores[Flourish::Context::FrameIndex()][1];
    }
}
