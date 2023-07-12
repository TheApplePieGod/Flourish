#include "flpch.h"
#include "ResourceSetAllocator.h"

#include "Flourish/Backends/Vulkan/Shader.h"
#include "Flourish/Backends/Vulkan/ResourceSet.h"
#include "Flourish/Backends/Vulkan/Util/DescriptorPool.h"

namespace Flourish::Vulkan
{
    // TODO: we could potentially get rid of this entire wrapper class and make
    // it DescriptorPool on its own
    ResourceSetAllocator::ResourceSetAllocator(const ResourceSetAllocatorCreateInfo& createInfo)
        : Flourish::ResourceSetAllocator(createInfo)
    {
        // TODO: check Compatability with subpass inputs (can only be fragment)

        if (m_Info.Layout.empty())
        {
            FL_LOG_ERROR("Cannot create ResourceSetAllocator with empty layout");
            throw std::exception();
        }

        std::sort(
            m_Info.Layout.begin(),
            m_Info.Layout.end(),
            [](const ResourceSetLayoutElement& a, const ResourceSetLayoutElement& b)
            {
                return a.BindingIndex < b.BindingIndex;
            }
        );

        // Create intermediate ReflectionData representation to be passed into DescriptorPool
        std::vector<ReflectionDataElement> reflectionData;
        reflectionData.reserve(m_Info.Layout.size());
        for (auto& elem : createInfo.Layout)
        {
            if (!reflectionData.empty() && reflectionData.back().BindingIndex == elem.BindingIndex)
            {
                FL_LOG_ERROR("Cannot create ResourceSetAllocator with duplicate binding indices");
                throw std::exception();
            }

            reflectionData.emplace_back(
                elem.ResourceType,
                elem.AccessFlags,
                elem.BindingIndex,
                0, 0, // Not used in DescriptorPool
                elem.ArrayCount
            );
        }

        m_Pool = std::make_shared<DescriptorPool>(reflectionData);
    }

    ResourceSetAllocator::~ResourceSetAllocator()
    {

    }

    std::shared_ptr<Flourish::ResourceSet> ResourceSetAllocator::Allocate(const ResourceSetCreateInfo& createInfo)
    {
        return std::make_shared<ResourceSet>(createInfo, m_Info.Compatability, m_Pool);
    }
}
