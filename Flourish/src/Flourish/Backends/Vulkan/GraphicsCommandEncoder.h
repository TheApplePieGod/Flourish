#pragma once

#include "Flourish/Api/GraphicsCommandEncoder.h"
#include "Flourish/Backends/Vulkan/Util/Common.h"
#include "Flourish/Backends/Vulkan/Util/Commands.h"

namespace Flourish::Vulkan
{
    class CommandBuffer;
    class GraphicsCommandEncoder : public Flourish::GraphicsCommandEncoder 
    {
    public:
        GraphicsCommandEncoder() = default;
        GraphicsCommandEncoder(CommandBuffer* parentBuffer, bool frameRestricted);

        void BeginEncoding();
        void EndEncoding() override;
        void GenerateMipMaps(Flourish::Texture* texture, SamplerFilter filter) override;
        void BlitTexture(Flourish::Texture* src, Flourish::Texture* dst, u32 srcLayerIndex, u32 srcMipLevel, u32 dstLayerIndex, u32 dstMipLevel) override;

        // TS
        inline VkCommandBuffer GetCommandBuffer() const { return m_CommandBuffer; }
        inline void MarkManuallyRecorded() { m_AnyCommandRecorded = true; }

    private:
        bool m_AnyCommandRecorded = false;
        bool m_FrameRestricted;
        VkCommandBuffer m_CommandBuffer;
        CommandBufferEncoderSubmission m_Submission;
        CommandBuffer* m_ParentBuffer;
    };
}
