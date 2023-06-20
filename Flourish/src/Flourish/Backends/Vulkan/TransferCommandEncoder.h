#pragma once

#include "Flourish/Api/TransferCommandEncoder.h"
#include "Flourish/Backends/Vulkan/Util/Common.h"
#include "Flourish/Backends/Vulkan/Util/Commands.h"

namespace Flourish::Vulkan
{
    class CommandBuffer;
    class TransferCommandEncoder : public Flourish::TransferCommandEncoder 
    {
    public:
        TransferCommandEncoder() = default;
        TransferCommandEncoder(CommandBuffer* parentBuffer, bool frameRestricted);

        void BeginEncoding();
        void EndEncoding() override;
        void FlushBuffer(Flourish::Buffer* buffer) override;
        void CopyTextureToBuffer(Flourish::Texture* texture, Flourish::Buffer* buffer, u32 layerIndex, u32 mipLevel) override;
        void CopyBufferToTexture(Flourish::Texture* texture, Flourish::Buffer* buffer, u32 layerIndex, u32 mipLevel) override;

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
