#include "flpch.h"
#include "DescriptorSetAllocator.h"

#include "Flourish/Backends/Vulkan/Shader.h"
#include "Flourish/Backends/Vulkan/DescriptorSet.h"
#include "Flourish/Backends/Vulkan/Util/DescriptorPool.h"

namespace Flourish::Vulkan
{
    // TODO: we could potentially get rid of this entire wrapper class and make
    // it DescriptorPool on its own
    DescriptorSetAllocator::DescriptorSetAllocator(const DescriptorSetAllocatorCreateInfo& createInfo)
        : Flourish::DescriptorSetAllocator(createInfo)
    {
        // TODO: check Compatability with subpass inputs (can only be fragment)

        if (m_Info.Layout.empty())
        {
            FL_LOG_ERROR("Cannot create DescriptorSetAllocator with empty layout");
            throw std::exception();
        }

        std::sort(
            m_Info.Layout.begin(),
            m_Info.Layout.end(),
            [](const DescriptorSetLayoutElement& a, const DescriptorSetLayoutElement& b)
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
                FL_LOG_ERROR("Cannot create DescriptorSetAllocator with duplicate binding indices");
                throw std::exception();
            }

            reflectionData.emplace_back(
                elem.ResourceType,
                m_Info.Compatability,
                elem.BindingIndex,
                0, 0, // Not used in DescriptorPool
                elem.ArrayCount
            );
        }

        m_Pool = std::make_shared<DescriptorPool>(reflectionData);
    }

    DescriptorSetAllocator::~DescriptorSetAllocator()
    {

    }

    std::shared_ptr<Flourish::DescriptorSet> DescriptorSetAllocator::Allocate(const DescriptorSetCreateInfo& createInfo)
    {
        return std::make_shared<DescriptorSet>(createInfo, m_Info.Compatability, m_Pool);
    }
}
