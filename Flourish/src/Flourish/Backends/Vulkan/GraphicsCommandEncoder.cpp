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

        m_AllocInfo = Context::Commands().AllocateBuffers(
            GPUWorkloadType::Graphics,
            false,
            &m_CommandBuffer,
            1, !m_FrameRestricted
        );
    
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        // TODO: check result?
        vkBeginCommandBuffer(m_CommandBuffer, &beginInfo);
    }

    void GraphicsCommandEncoder::EndEncoding()
    {
        FL_CRASH_ASSERT(m_Encoding, "Cannot end encoding that has already ended");
        m_Encoding = false;

        VkCommandBuffer buffer = m_CommandBuffer;
        vkEndCommandBuffer(buffer);
        m_ParentBuffer->SubmitEncodedCommands(buffer, m_AllocInfo, GPUWorkloadType::Graphics);
    }

    void GraphicsCommandEncoder::GenerateMipMaps(Flourish::Texture* _texture)
    {
        FL_CRASH_ASSERT(m_Encoding, "Cannot encode GenerateMipMaps after encoding has ended");
        
        Texture* texture = static_cast<Texture*>(_texture);

        Texture::GenerateMipmaps(
            texture->GetImage(),
            Common::ConvertColorFormat(texture->GetColorFormat()),
            texture->GetWidth(),
            texture->GetHeight(),
            texture->GetMipCount(),
            texture->GetArrayCount(),
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_FILTER_LINEAR,
            m_CommandBuffer
        );
    }
}