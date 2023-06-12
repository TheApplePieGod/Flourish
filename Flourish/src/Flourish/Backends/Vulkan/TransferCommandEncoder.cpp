#include "flpch.h"
#include "TransferCommandEncoder.h"

#include "Flourish/Backends/Vulkan/Context.h"
#include "Flourish/Backends/Vulkan/CommandBuffer.h"
#include "Flourish/Backends/Vulkan/Texture.h"
#include "Flourish/Backends/Vulkan/Buffer.h"

namespace Flourish::Vulkan
{
    TransferCommandEncoder::TransferCommandEncoder(CommandBuffer* parentBuffer, bool frameRestricted)
        : m_ParentBuffer(parentBuffer), m_FrameRestricted(frameRestricted)
    {}

    void TransferCommandEncoder::BeginEncoding()
    {
        m_Encoding = true;

        m_Submission.Buffers.resize(1);
        m_Submission.AllocInfo = Context::Commands().AllocateBuffers(
            GPUWorkloadType::Transfer,
            true,
            m_Submission.Buffers.data(),
            m_Submission.Buffers.size(),
            !m_FrameRestricted
        );   
        m_CommandBuffer = m_Submission.Buffers[0];
    
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        // TODO: check result?
        vkBeginCommandBuffer(m_CommandBuffer, &beginInfo);
    }

    void TransferCommandEncoder::EndEncoding()
    {
        FL_CRASH_ASSERT(m_Encoding, "Cannot end encoding that has already ended");
        m_Encoding = false;

        vkEndCommandBuffer(m_CommandBuffer);
        m_ParentBuffer->SubmitEncodedCommands(m_Submission);
    }

    void TransferCommandEncoder::FlushBuffer(Flourish::Buffer* _buffer)
    {
        FL_CRASH_ASSERT(m_Encoding, "Cannot encode FlushBuffer after encoding has ended");

        Buffer* buffer = static_cast<Buffer*>(_buffer);

        buffer->FlushInternal(m_CommandBuffer);
    }

    void TransferCommandEncoder::CopyTextureToBuffer(Flourish::Texture* _texture, Flourish::Buffer* _buffer, u32 layerIndex, u32 mipLevel)
    {
        FL_CRASH_ASSERT(m_Encoding, "Cannot encode CopyTextureToBuffer after encoding has ended");
        FL_ASSERT(_buffer->GetType() == BufferType::Pixel, "Only pixel buffers may be the targets of CopyTextureToBuffer");
        
        Texture* texture = static_cast<Texture*>(_texture);
        Buffer* buffer = static_cast<Buffer*>(_buffer);
        
        VkImageLayout startingLayout = texture->GetUsageType() == TextureUsageType::ComputeTarget ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkImageAspectFlags aspect = texture->IsDepthImage() ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

        Texture::TransitionImageLayout(
            texture->GetImage(),
            startingLayout,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            aspect,
            mipLevel, 1,
            layerIndex, 1,
            0, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            m_CommandBuffer
        );

        Buffer::CopyImageToBuffer(
            texture->GetImage(),
            buffer->GetBuffer(),
            texture->GetWidth(),
            texture->GetHeight(),
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            m_CommandBuffer
        );

        Texture::TransitionImageLayout(
            texture->GetImage(),
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            startingLayout,
            aspect,
            mipLevel, 1,
            layerIndex, 1,
            VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            0, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            m_CommandBuffer
        );
    }

    void TransferCommandEncoder::CopyBufferToTexture(Flourish::Texture* _texture, Flourish::Buffer* _buffer, u32 layerIndex, u32 mipLevel)
    {
        FL_CRASH_ASSERT(m_Encoding, "Cannot encode CopyBufferToTexture after encoding has ended");
        
        Texture* texture = static_cast<Texture*>(_texture);
        Buffer* buffer = static_cast<Buffer*>(_buffer);

        VkImageLayout startingLayout = texture->GetUsageType() == TextureUsageType::ComputeTarget ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkImageAspectFlags aspect = texture->IsDepthImage() ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

        Texture::TransitionImageLayout(
            texture->GetImage(),
            startingLayout,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            aspect,
            mipLevel, 1,
            layerIndex, 1,
            0, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            m_CommandBuffer
        );

        Buffer::CopyBufferToImage(
            buffer->GetBuffer(),
            texture->GetImage(),
            texture->GetWidth(),
            texture->GetHeight(),
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            m_CommandBuffer
        );

        Texture::TransitionImageLayout(
            texture->GetImage(),
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            startingLayout,
            aspect,
            mipLevel, 1,
            layerIndex, 1,            
            VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            0, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            m_CommandBuffer
        );
    }
}
