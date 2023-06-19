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

    void GraphicsCommandEncoder::GenerateMipMaps(Flourish::Texture* _texture)
    {
        FL_CRASH_ASSERT(m_Encoding, "Cannot encode GenerateMipMaps after encoding has ended");
        
        Texture* texture = static_cast<Texture*>(_texture);

        VkImageAspectFlags aspect = texture->IsDepthImage() ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

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
            VK_FILTER_LINEAR,
            m_CommandBuffer
        );
        m_AnyCommandRecorded = true;
    }
}
