#include "flpch.h"
#include "ResourceSetAllocator.h"

#include "Flourish/Backends/Vulkan/ResourceSetAllocator.h"

namespace Flourish
{
    ResourceSetAllocator::ResourceSetAllocator(const ResourceSetAllocatorCreateInfo& createInfo)
        : m_Info(createInfo)
    {
        // Validate access flags with pipeline compatability
        #ifdef FL_DEBUG
        for (auto& elem : m_Info.Layout)
        {
            if (m_Info.Compatability & ResourceSetPipelineCompatabilityFlags::Compute)
            {
                FL_ASSERT(
                    elem.AccessFlags == ShaderTypeFlags::Compute,
                    "Resource set allocator with Compute compatability must only have compute access types"
                );
            }
            else if (m_Info.Compatability & ResourceSetPipelineCompatabilityFlags::Graphics)
            {
                FL_ASSERT(
                    (elem.AccessFlags & ~(ShaderTypeFlags::Vertex | ShaderTypeFlags::Fragment)) == 0,
                    "Resource set allocator with Graphics compatability must only have vertex or fragment access types"
                );
            }
            else if (m_Info.Compatability & ResourceSetPipelineCompatabilityFlags::RayTracing)
            {
                FL_ASSERT(
                    (elem.AccessFlags & ~(
                        ShaderTypeFlags::RayGen |
                        ShaderTypeFlags::RayMiss |
                        ShaderTypeFlags::RayAnyHit |
                        ShaderTypeFlags::RayClosestHit |
                        ShaderTypeFlags::RayIntersection
                    )) == 0,
                    "Resource set allocator with RayTracing compatability must only have ray-prefixed access types"
                );
            }
        }
        #endif
    }

    std::shared_ptr<ResourceSetAllocator> ResourceSetAllocator::Create(const ResourceSetAllocatorCreateInfo& createInfo)
    {
        FL_ASSERT(Context::BackendType() != BackendType::None, "Must initialize Context before creating a ResourceSetAllocator");

        try
        {
            switch (Context::BackendType())
            {
                case BackendType::Vulkan: { return std::make_shared<Vulkan::ResourceSetAllocator>(createInfo); }
            }
        }
        catch (const std::exception& e) {}

        FL_ASSERT(false, "Failed to create ResourceSetAllocator");
        return nullptr;
    }
}
