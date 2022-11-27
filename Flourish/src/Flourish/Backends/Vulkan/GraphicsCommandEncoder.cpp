#include "flpch.h"
#include "GraphicsCommandEncoder.h"

#include "Flourish/Backends/Vulkan/Util/Context.h"
#include "Flourish/Backends/Vulkan/CommandBuffer.h"
#include "Flourish/Backends/Vulkan/Texture.h"

namespace Flourish::Vulkan
{
    GraphicsCommandEncoder::GraphicsCommandEncoder(CommandBuffer* parentBuffer)
    {
        m_ParentBuffer = parentBuffer;
        Context::Commands().AllocateBuffers(
            GPUWorkloadType::Graphics,
            false,
            m_CommandBuffers.data(),
            Flourish::Context::FrameBufferCount()
        );   
    }

    GraphicsCommandEncoder::~GraphicsCommandEncoder()
    {
        // We shouldn't have to do any thread sanity checking here because command buffer
        // already does this and it is the only class who will own this object. Also, FreeBuffers()
        // already handles a delete queue entry
        std::vector<VkCommandBuffer> buffers(m_CommandBuffers.begin(), m_CommandBuffers.begin() + Flourish::Context::FrameBufferCount());
        Context::DeleteQueue().Push([buffers]()
        {
            Context::Commands().FreeBuffers(
                GPUWorkloadType::Graphics,
                buffers
            );
        }, "Graphics command encoder free");
    }

    void GraphicsCommandEncoder::BeginEncoding()
    {
        m_Encoding = true;
    
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        // TODO: check result?
        VkCommandBuffer buffer = GetCommandBuffer();
        vkResetCommandBuffer(buffer, 0);
        vkBeginCommandBuffer(buffer, &beginInfo);
    }

    void GraphicsCommandEncoder::EndEncoding()
    {
        FL_CRASH_ASSERT(m_Encoding, "Cannot end encoding that has already ended");
        m_Encoding = false;

        VkCommandBuffer buffer = GetCommandBuffer();
        vkEndCommandBuffer(buffer);
        m_ParentBuffer->SubmitEncodedCommands(buffer, GPUWorkloadType::Graphics);
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
            GetCommandBuffer()
        );
    }

    VkCommandBuffer GraphicsCommandEncoder::GetCommandBuffer() const
    {
        return m_CommandBuffers[Flourish::Context::FrameIndex()];
    }
}