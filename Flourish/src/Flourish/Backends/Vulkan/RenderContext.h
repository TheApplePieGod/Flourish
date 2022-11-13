#pragma once

#include "Flourish/Api/RenderContext.h"
#include "Flourish/Backends/Vulkan/Util/Swapchain.h"
#include "Flourish/Backends/Vulkan/Util/Common.h"
#include "Flourish/Backends/Vulkan/CommandBuffer.h"

namespace Flourish::Vulkan
{
    struct RenderContextSubmissionData
    {
        std::vector<VkSemaphore> WaitSemaphores;
        std::vector<u64> WaitSemaphoreValues;
        std::vector<VkPipelineStageFlags> WaitStages;
        std::array<VkSemaphore, Flourish::Context::MaxFrameBufferCount> SignalSemaphores;
        VkSubmitInfo SubmitInfo;
        VkTimelineSemaphoreSubmitInfo TimelineSubmitInfo;
    };

    class RenderContext : public Flourish::RenderContext
    {
    public:
        RenderContext(const RenderContextCreateInfo& createInfo);
        ~RenderContext() override;

        void Present(const std::vector<std::vector<Flourish::CommandBuffer*>>& dependencyBuffers) override;
        RenderPass* GetRenderPass() const override;

        // TODO: this can only be allowed once per frame
        [[nodiscard]] Flourish::RenderCommandEncoder* EncodeFrameRenderCommands() override; 

        // TS
        inline VkSurfaceKHR Surface() const { return m_Surface; }
        inline const Vulkan::Swapchain& Swapchain() const { return m_Swapchain; }
        inline const Vulkan::CommandBuffer& CommandBuffer() const { return m_CommandBuffer; }
        inline VkSemaphore GetImageAvailableSemaphore() const { return m_Swapchain.GetImageAvailableSemaphore(); }
        inline const RenderContextSubmissionData& GetSubmissionData() const { return m_SubmissionData; }

    private:
    
    private:
        VkSurfaceKHR m_Surface;
        Vulkan::Swapchain m_Swapchain;
        Vulkan::CommandBuffer m_CommandBuffer;
        RenderContextSubmissionData m_SubmissionData;
        u64 m_LastEncodingFrame = 0;
        u64 m_LastPresentFrame = 0;
    };
}