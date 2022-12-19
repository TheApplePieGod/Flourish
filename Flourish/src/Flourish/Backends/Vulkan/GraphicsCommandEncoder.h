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
        void GenerateMipMaps(Flourish::Texture* texture) override;

        // TS
        inline VkCommandBuffer GetCommandBuffer() const { return m_CommandBuffer; }

    private:
        bool m_FrameRestricted;
        CommandBufferAllocInfo m_AllocInfo;
        VkCommandBuffer m_CommandBuffer;
        CommandBuffer* m_ParentBuffer;
    };
}