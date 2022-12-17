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
        std::array<std::array<VkSemaphore, 2>, Flourish::Context::MaxFrameBufferCount> SignalSemaphores;
        std::array<std::array<u64, 2>, Flourish::Context::MaxFrameBufferCount> SignalSemaphoreValues;
    };

    class RenderContext : public Flourish::RenderContext
    {
    public:
        RenderContext(const RenderContextCreateInfo& createInfo);
        ~RenderContext() override;

        void Present(const std::vector<std::vector<Flourish::CommandBuffer*>>& dependencyBuffers) override;
        void UpdateDimensions(u32 width, u32 height) override;
        RenderPass* GetRenderPass() const override;
        bool Validate() override;

        [[nodiscard]] Flourish::RenderCommandEncoder* EncodeRenderCommands() override; 

        // TS
        inline VkSurfaceKHR Surface() const { return m_Surface; }
        inline Vulkan::Swapchain& Swapchain() { return m_Swapchain; }
        inline Vulkan::CommandBuffer& CommandBuffer() { return m_CommandBuffer; }
        inline VkSemaphore GetImageAvailableSemaphore() const { return m_Swapchain.GetImageAvailableSemaphore(); }
        inline const RenderContextSubmissionData& GetSubmissionData() const { return m_SubmissionData; }

    private:
        VkSurfaceKHR m_Surface;
        Vulkan::Swapchain m_Swapchain;
        Vulkan::CommandBuffer m_CommandBuffer;
        RenderContextSubmissionData m_SubmissionData;
        u64 m_LastEncodingFrame = 0;
        u64 m_LastPresentFrame = 0;
    };
}