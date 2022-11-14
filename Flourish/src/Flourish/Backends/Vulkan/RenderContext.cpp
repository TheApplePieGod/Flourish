#include "flpch.h"
#include "RenderContext.h"

#include "Flourish/Backends/Vulkan/Util/Context.h"
#include "Flourish/Backends/Vulkan/Util/Synchronization.h"

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
        });
    }

    void RenderContext::Present(const std::vector<std::vector<Flourish::CommandBuffer*>>& dependencyBuffers)
    {
        FL_CRASH_ASSERT(m_LastEncodingFrame == Flourish::Context::FrameCount(), "Cannot present render context that has not been encoded");
        if (m_LastPresentFrame == Flourish::Context::FrameCount())
        {
            FL_ASSERT(false, "Cannot present render context multiple times per frame");
            return;
        }
        m_LastPresentFrame = Flourish::Context::FrameCount();
        
        Flourish::Context::SubmitCommandBuffers(dependencyBuffers);
        Context::SubmissionHandler().PresentRenderContext(this);

        m_SubmissionData.WaitSemaphores.clear();
        m_SubmissionData.WaitSemaphoreValues.clear();
        m_SubmissionData.WaitStages.clear();
        
        // Add semaphore to wait for the image to be available
        m_SubmissionData.WaitSemaphores.push_back(GetImageAvailableSemaphore());
        m_SubmissionData.WaitSemaphoreValues.push_back(Flourish::Context::FrameCount());
        m_SubmissionData.WaitStages.push_back(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

        for (auto& _buffer : dependencyBuffers.back())
        {
            Vulkan::CommandBuffer* buffer = static_cast<Vulkan::CommandBuffer*>(_buffer);
            if (buffer->GetEncoderSubmissions().empty()) continue; // TODO: warn here?

            auto& subData = buffer->GetSubmissionData();
                    
            // Add each final sub buffer semaphore to wait on
            m_SubmissionData.WaitSemaphores.push_back(subData.SyncSemaphores[Flourish::Context::FrameIndex()]);
            m_SubmissionData.WaitSemaphoreValues.push_back(subData.SyncSemaphoreValues.back());
            m_SubmissionData.WaitStages.push_back(subData.FinalSubBufferWaitStage);
        }

        m_SubmissionData.TimelineSubmitInfo.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
        m_SubmissionData.TimelineSubmitInfo.waitSemaphoreValueCount = m_SubmissionData.WaitSemaphoreValues.size();
        m_SubmissionData.TimelineSubmitInfo.pWaitSemaphoreValues = m_SubmissionData.WaitSemaphoreValues.data();
        // We need this even though we aren't signaling a timeline semaphore. Removing this causes
        // an extremely hard to find crash on macos
        m_SubmissionData.TimelineSubmitInfo.signalSemaphoreValueCount = 1;
        m_SubmissionData.TimelineSubmitInfo.pSignalSemaphoreValues = m_SubmissionData.WaitSemaphoreValues.data();

        m_SubmissionData.SubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        m_SubmissionData.SubmitInfo.pNext = &m_SubmissionData.TimelineSubmitInfo;
        m_SubmissionData.SubmitInfo.waitSemaphoreCount = m_SubmissionData.WaitSemaphores.size();
        m_SubmissionData.SubmitInfo.pWaitSemaphores = m_SubmissionData.WaitSemaphores.data();
        m_SubmissionData.SubmitInfo.pWaitDstStageMask = m_SubmissionData.WaitStages.data();
        m_SubmissionData.SubmitInfo.signalSemaphoreCount = 1;
        m_SubmissionData.SubmitInfo.pSignalSemaphores = &m_SubmissionData.SignalSemaphores[Flourish::Context::FrameIndex()];
        m_SubmissionData.SubmitInfo.commandBufferCount = 1;
        m_SubmissionData.SubmitInfo.pCommandBuffers = &m_CommandBuffer.GetEncoderSubmissions()[0].Buffer;
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