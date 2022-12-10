#pragma once

#include "Flourish/Api/GraphicsCommandEncoder.h"
#include "Flourish/Backends/Vulkan/Util/Common.h"
#include "Flourish/Backends/Vulkan/Util/DescriptorSet.h"
#include "Flourish/Backends/Vulkan/Util/Commands.h"
#include "Flourish/Backends/Vulkan/ComputePipeline.h"

namespace Flourish::Vulkan
{
    class CommandBuffer;
    class GraphicsCommandEncoder : public Flourish::GraphicsCommandEncoder 
    {
    public:
        GraphicsCommandEncoder(CommandBuffer* parentBuffer);
        ~GraphicsCommandEncoder();

        void BeginEncoding();
        void EndEncoding() override;
        void GenerateMipMaps(Flourish::Texture* texture) override;
        
        // TS
        VkCommandBuffer GetCommandBuffer() const;

    private:
        std::array<VkCommandBuffer, Flourish::Context::MaxFrameBufferCount> m_CommandBuffers;
        CommandBufferAllocInfo m_AllocInfo;
        CommandBuffer* m_ParentBuffer;
    };
}