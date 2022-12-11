#include "flpch.h"
#include "RenderContext.h"

#include "Flourish/Backends/Vulkan/Util/Context.h"
#include "Flourish/Backends/Vulkan/Util/Synchronization.h"

#ifdef FL_USE_GLFW
#include "GLFW/glfw3.h"
#endif

namespace Flourish::Vulkan
{
    RenderContext::RenderContext(const RenderContextCreateInfo& createInfo)
        : Flourish::RenderContext(createInfo),
         m_CommandBuffer({}, false)
    {
        auto instance = Context::Instance();

        // Create the surface
        #ifdef FL_USE_GLFW
            glfwCreateWindowSurface(instance, createInfo.Window, nullptr, &m_Surface);
        #elif defined(FL_PLATFORM_WINDOWS)
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
        
        for (u32 frame = 0; frame < Flourish::Context::FrameBufferCount(); frame++)
            m_SubmissionData.SignalSemaphores[frame] = Synchronization::CreateSemaphore();
    }

    RenderContext::~RenderContext()
    {
        m_Swapchain.Shutdown();

        auto surface = m_Surface;
        auto semaphores = m_SubmissionData.SignalSemaphores;
        Context::DeleteQueue().Push([=]()
        {
            vkDestroySurfaceKHR(Context::Instance(), surface, nullptr);
            for (u32 frame = 0; frame < Flourish::Context::FrameBufferCount(); frame++)
                vkDestroySemaphore(Context::Devices().Device(), semaphores[frame], nullptr);
        }, "Render context free");
    }

    void RenderContext::Present(const std::vector<std::vector<std::vector<Flourish::CommandBuffer*>>>& dependencyBuffers)
    {
        FL_CRASH_ASSERT(m_LastEncodingFrame == Flourish::Context::FrameCount(), "Cannot present render context that has not been encoded");
        if (m_LastPresentFrame == Flourish::Context::FrameCount())
        {
            FL_ASSERT(false, "Cannot present render context multiple times per frame");
            return;
        }
        m_LastPresentFrame = Flourish::Context::FrameCount();
        
        if (!m_Swapchain.IsValid()) return;

        if (!m_CommandBuffer.GetEncoderSubmissions().empty())
        {
            m_SubmissionData.WaitSemaphores.clear();
            m_SubmissionData.WaitSemaphoreValues.clear();
            m_SubmissionData.WaitStages.clear();
            
            // Add semaphore to wait for the image to be available
            m_SubmissionData.WaitSemaphores.push_back(GetImageAvailableSemaphore());
            m_SubmissionData.WaitSemaphoreValues.push_back(Flourish::Context::FrameCount());
            m_SubmissionData.WaitStages.push_back(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

            for (auto& submission : dependencyBuffers)
            {
                for (auto& _buffer : submission.back())
                {
                    Vulkan::CommandBuffer* buffer = static_cast<Vulkan::CommandBuffer*>(_buffer);
                    if (buffer->GetEncoderSubmissions().empty()) continue; // TODO: warn here?

                    auto& subData = buffer->GetSubmissionData();
                            
                    // Add each final sub buffer semaphore to wait on
                    m_SubmissionData.WaitSemaphores.push_back(subData.SyncSemaphores[Flourish::Context::FrameIndex()]);
                    m_SubmissionData.WaitSemaphoreValues.push_back(buffer->GetFinalSemaphoreValue());
                    m_SubmissionData.WaitStages.push_back(subData.FinalSubBufferWaitStage);
                }

                Flourish::Context::SubmitCommandBuffers(submission);
            }
            
            // Update the first encoded command to wait until the image is available
            auto& subData = m_CommandBuffer.GetSubmissionData();
            subData.FirstSubmitInfo->waitSemaphoreCount = m_SubmissionData.WaitSemaphores.size();
            subData.FirstSubmitInfo->pWaitSemaphores = m_SubmissionData.WaitSemaphores.data();
            subData.FirstSubmitInfo->pWaitDstStageMask = m_SubmissionData.WaitStages.data();
            subData.TimelineSubmitInfos.front().waitSemaphoreValueCount = m_SubmissionData.WaitSemaphores.size();
            subData.TimelineSubmitInfos.front().pWaitSemaphoreValues = m_SubmissionData.WaitSemaphoreValues.data();

            // Update last encoded command to signal the final binary semaphore needed to present
            subData.LastSubmitInfo->signalSemaphoreCount = 1;
            subData.LastSubmitInfo->pSignalSemaphores = &m_SubmissionData.SignalSemaphores[Flourish::Context::FrameIndex()];
            // We need this even though we aren't signaling a timeline semaphore. Removing this causes
            // an extremely hard to find crash on macos
            subData.TimelineSubmitInfos.back().signalSemaphoreValueCount = 1;
            subData.TimelineSubmitInfos.back().pSignalSemaphoreValues = m_SubmissionData.WaitSemaphoreValues.data();
        }

        Context::SubmissionHandler().PresentRenderContext(this);
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
        }

        return m_CommandBuffer.EncodeRenderCommands(m_Swapchain.GetFramebuffer());
    } 
}