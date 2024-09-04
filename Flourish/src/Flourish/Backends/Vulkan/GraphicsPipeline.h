#pragma once

#include "Flourish/Api/GraphicsPipeline.h"
#include "Flourish/Backends/Vulkan/Util/Common.h"
#include "Flourish/Backends/Vulkan/Util/DescriptorBinder.h"

namespace Flourish::Vulkan
{
    class RenderPass;
    class GraphicsPipeline : public Flourish::GraphicsPipeline
    {
    public:
        GraphicsPipeline(
            const GraphicsPipelineCreateInfo& createInfo,
            RenderPass* renderPass
        );
        ~GraphicsPipeline() override;

        bool ValidateShaders() override;

        // TS
        std::shared_ptr<Flourish::ResourceSet> CreateResourceSet(u32 setIndex, const ResourceSetCreateInfo& createInfo) override;
        
        // TS
        inline VkPipelineLayout GetLayout() const { return m_PipelineLayout; }
        inline VkPipeline GetPipeline(u32 subpassIndex) { return m_Pipelines[subpassIndex]; };
        inline const PipelineDescriptorData* GetDescriptorData() const { return &m_DescriptorData; }

    private:
        void Recreate();
        void Cleanup();

    private:
        bool m_Created = false;
        VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;
        std::unordered_map<u32, VkPipeline> m_Pipelines;
        PipelineDescriptorData m_DescriptorData;
        std::array<u32, 2> m_ShaderRevisions;

        // We can store this safely here because the lifetime of this pipeline is tied
        // to the lifetime of the parent pass
        RenderPass* m_RenderPass;
    };
}
