#pragma once

#include "Flourish/Api/RayTracing/RayTracingPipeline.h"
#include "Flourish/Backends/Vulkan/Util/Common.h"
#include "Flourish/Backends/Vulkan/Util/DescriptorBinder.h"

namespace Flourish::Vulkan
{
    class RayTracingPipeline : public Flourish::RayTracingPipeline
    {
    public:
        RayTracingPipeline(const RayTracingPipelineCreateInfo& createInfo);
        ~RayTracingPipeline() override;

        // TS
        std::shared_ptr<Flourish::ResourceSet> CreateResourceSet(u32 setIndex, const ResourceSetCreateInfo& createInfo) override;

        // TS
        u8* GetGroupHandle(u32 groupIndex);
        
        // TS
        inline VkPipeline GetPipeline() const { return m_Pipeline; }
        inline VkPipelineLayout GetLayout() const { return m_PipelineLayout; }
        inline const PipelineDescriptorData* GetDescriptorData() const { return &m_DescriptorData; }

    private:
        VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;
        VkPipeline m_Pipeline = VK_NULL_HANDLE;
        PipelineDescriptorData m_DescriptorData;
        std::vector<u8> m_GroupHandles;
    };
}
