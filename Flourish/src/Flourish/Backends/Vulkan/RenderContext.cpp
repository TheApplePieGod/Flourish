#include "flpch.h"
#include "RenderContext.h"

#include "Flourish/Backends/Vulkan/Util/Context.h"

namespace Flourish::Vulkan
{
    RenderContext::RenderContext(const RenderContextCreateInfo& createInfo)
        : Flourish::RenderContext(createInfo),
         m_CommandBuffer({}, false)
    {
        auto instance = Context::Instance();

        // Create the surface
        #ifdef FL_PLATFORM_WINDOWS
            VkWin32SurfaceCreateInfoKHR surfaceInfo{};
            surfaceInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
            surfaceInfo.pNext = nullptr;
            surfaceInfo.hinstance = createInfo.Instance;
            surfaceInfo.hwnd = createInfo.Window;

            vkCreateWin32SurfaceKHR(instance, &surfaceInfo, nullptr, &m_Surface);
        #elif defined(FL_PLATFORM_LINUX)
            VkXcbSurfaceCreateInfoKHR surfaceInfo{};
            surfaceInfo.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
            surfaceInfo.pNext = nullptr;
            surfaceInfo.connection = createInfo.connection;
            surfaceInfo.window = createInfo.window;

            vkCreateXcbSurfaceKHR(instance, &surfaceInfo, nullptr, &m_Surface);
        #else
            VkMacOSSurfaceCreateInfoMVK surfaceInfo{};
            surfaceInfo.sType = VK_STRUCTURE_TYPE_MACOS_SURFACE_CREATE_INFO_MVK;
            surfaceInfo.pView = createInfo.NSView;

            vkCreateMacOSSurfaceMVK(instance, &surfaceInfo, nullptr, &m_Surface);
        #endif

        FL_ASSERT(m_Surface, "Unable to create window surface");
        if (!m_Surface) return;

        m_Swapchain.Initialize(createInfo, m_Surface);
    }

    RenderContext::~RenderContext()
    {
        m_Swapchain.Shutdown();

        auto surface = m_Surface;
        Context::DeleteQueue().Push([=]()
        {
            vkDestroySurfaceKHR(Context::Instance(), surface, nullptr);
        });
    }

    void RenderContext::Present(const std::vector<std::vector<const Flourish::CommandBuffer*>>& dependencyBuffers)
    {
        FL_CRASH_ASSERT(m_LastEncodingFrame == Flourish::Context::FrameCount(), "Cannot present render context that has not been encoded");
        if (m_LastPresentFrame == Flourish::Context::FrameCount())
        {
            FL_ASSERT(false, "Cannot present render context multiple times per frame");
            return;
        }

        m_LastPresentFrame = Flourish::Context::FrameCount();

        u32 submissionId = Flourish::Context::SubmitCommandBuffers(dependencyBuffers);
        Context::SubmissionHandler().PresentRenderContext(this, submissionId);
    }

    RenderPass* RenderContext::GetRenderPass() const
    {
        return m_Swapchain.GetRenderPass();
    }

    Flourish::RenderCommandEncoder* RenderContext::EncodeFrameRenderCommands()
    {
        if (m_LastEncodingFrame == Flourish::Context::FrameCount())
        {
            FL_ASSERT(false, "Cannot encode frame render commands multiple times per frame");
            return nullptr;
        }

        m_LastEncodingFrame = Flourish::Context::FrameCount();

        m_Swapchain.UpdateActiveImage();
        return m_CommandBuffer.EncodeRenderCommands(m_Swapchain.GetFramebuffer());
    } 
}