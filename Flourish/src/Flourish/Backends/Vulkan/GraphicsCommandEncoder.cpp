#include "flpch.h"
#include "GraphicsCommandEncoder.h"

#include "Flourish/Backends/Vulkan/Context.h"
#include "Flourish/Backends/Vulkan/CommandBuffer.h"
#include "Flourish/Backends/Vulkan/Texture.h"

namespace Flourish::Vulkan
{
    GraphicsCommandEncoder::GraphicsCommandEncoder(CommandBuffer* parentBuffer, bool frameRestricted)
        : m_ParentBuffer(parentBuffer), m_FrameRestricted(frameRestricted)
    {}

    void GraphicsCommandEncoder::BeginEncoding()
    {
        m_Encoding = true;
        m_AnyCommandRecorded = false;

        m_Submission.Buffers.resize(1);
        m_Submission.AllocInfo = Context::Commands().AllocateBuffers(
            GPUWorkloadType::Graphics,
            true,
            m_Submission.Buffers.data(),
            m_Submission.Buffers.size(),
            !m_FrameRestricted
        );   
        m_CommandBuffer = m_Submission.Buffers[0];
    
        VkCommandBufferInheritanceInfo inheritanceInfo{};
        inheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
    
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        beginInfo.pInheritanceInfo = &inheritanceInfo;

        // TODO: check result?
        vkBeginCommandBuffer(m_CommandBuffer, &beginInfo);
    }

    void GraphicsCommandEncoder::EndEncoding()
    {
        FL_CRASH_ASSERT(m_Encoding, "Cannot end encoding that has already ended");
        m_Encoding = false;

        vkEndCommandBuffer(m_CommandBuffer);

        if (!m_AnyCommandRecorded)
            m_Submission.Buffers.clear();

        m_ParentBuffer->SubmitEncodedCommands(m_Submission);
    }

    void GraphicsCommandEncoder::GenerateMipMaps(Flourish::Texture* _texture, SamplerFilter filter)
    {
        FL_CRASH_ASSERT(m_Encoding, "Cannot encode GenerateMipMaps after encoding has ended");
        FL_ASSERT(_texture->GetUsageType() & TextureUsageFlags::Transfer, "Texture must be created with transfer flag to generate mipmaps");
        
        Texture* texture = static_cast<Texture*>(_texture);

        VkImageAspectFlags aspect = texture->IsDepthImage() ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
        FL_ASSERT(
            filter == SamplerFilter::Nearest || !texture->IsDepthImage(),
            "Depth images can only generate mipmaps with the nearest sampler filter"
        );

        Texture::GenerateMipmaps(
            texture->GetImage(),
            Common::ConvertColorFormat(texture->GetColorFormat()),
            aspect,
            texture->GetWidth(),
            texture->GetHeight(),
            texture->GetMipCount(),
            texture->GetArrayCount(),
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            Common::ConvertSamplerFilter(filter),
            m_CommandBuffer
        );
        m_AnyCommandRecorded = true;
    }

    void GraphicsCommandEncoder::BlitTexture(Flourish::Texture* _src, Flourish::Texture* _dst, u32 srcLayerIndex, u32 srcMipLevel, u32 dstLayerIndex, u32 dstMipLevel)
    {
        FL_CRASH_ASSERT(m_Encoding, "Cannot encode CopyTextureToBuffer after encoding has ended");
        FL_ASSERT(_src->GetUsageType() & TextureUsageFlags::Transfer, "Texture src must be created with transfer flag to perform blit");
        FL_ASSERT(_dst->GetUsageType() & TextureUsageFlags::Transfer, "Texture dst must be created with transfer flag to perform blit");
        FL_ASSERT(_src->GetWidth() == _dst->GetWidth(), "Src and dst images must have same width to blit");
        FL_ASSERT(_src->GetHeight() == _dst->GetHeight(), "Src and dst images must have same height to blit");
        
        Texture* src = static_cast<Texture*>(_src);
        Texture* dst = static_cast<Texture*>(_dst);
        
        VkImageLayout srcLayout = (src->GetUsageType() & TextureUsageFlags::Compute) ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkImageLayout dstLayout = (src->GetUsageType() & TextureUsageFlags::Compute) ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkImageAspectFlags srcAspect = src->IsDepthImage() ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
        VkImageAspectFlags dstAspect = dst->IsDepthImage() ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

        Texture::TransitionImageLayout(
            src->GetImage(),
            srcLayout,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            srcAspect,
            srcMipLevel, 1,
            srcLayerIndex, 1,
            0, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            m_CommandBuffer
        );
        Texture::TransitionImageLayout(
            dst->GetImage(),
            dstLayout,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            dstAspect,
            dstMipLevel, 1,
            dstLayerIndex, 1,
            0, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            m_CommandBuffer
        );

        Texture::Blit(
            src->GetImage(),
            Common::ConvertColorFormat(src->GetColorFormat()),
            srcAspect,
            srcMipLevel,
            srcLayerIndex,
            dst->GetImage(),
            Common::ConvertColorFormat(dst->GetColorFormat()),
            dstAspect,
            dstMipLevel,
            dstLayerIndex,
            src->GetWidth(),
            src->GetHeight(),
            VK_FILTER_NEAREST, // Assumed for a copy
            m_CommandBuffer
        );

        Texture::TransitionImageLayout(
            dst->GetImage(),
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            dstLayout,
            dstAspect,
            dstMipLevel, 1,
            dstLayerIndex, 1,
            VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            0, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            m_CommandBuffer
        );
        Texture::TransitionImageLayout(
            src->GetImage(),
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            srcLayout,
            srcAspect,
            srcMipLevel, 1,
            srcLayerIndex, 1,
            VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            0, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            m_CommandBuffer
        );

        m_AnyCommandRecorded = true;
    }
}
