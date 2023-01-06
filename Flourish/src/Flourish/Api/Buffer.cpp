#include "flpch.h"
#include "Buffer.h"

#include "Flourish/Api/Context.h"
#include "Flourish/Backends/Vulkan/Buffer.h"

namespace Flourish
{
    BufferLayout::BufferLayout(std::initializer_list<BufferLayoutElement> elements)
        : m_Elements(elements)
    {
        m_CalculatedStride = CalculateStrideAndOffsets();
    }

    u32 BufferLayout::CalculateStrideAndOffsets()
    {
        u32 stride = 0;

        for (auto& element : m_Elements)
        {
            element.Offset = stride;
            stride += element.CalculatedSize;
        }

        if (stride % 4 != 0)
            FL_LOG_WARN("Buffer layout of length %d has stride %d that is not four byte aligned", m_Elements.size(), stride);

        return stride;
    }

    Buffer::Buffer(const BufferCreateInfo& createInfo)
        : m_Info(createInfo)
    {
        if (m_Info.Stride != 0 && m_Info.Stride % 4 != 0)
            FL_LOG_WARN("Buffer has explicit stride %d that is not four byte aligned", m_Info.Stride);
    }

    void Buffer::SetElements(const void* data, u32 elementCount, u32 elementOffset)
    {
        FL_ASSERT(elementCount + elementOffset <= m_Info.ElementCount, "Attempting to set data on buffer which is larger than allocated size");
        SetBytes(data, GetStride() * elementCount, GetStride() * elementOffset);
    }

    std::shared_ptr<Buffer> Buffer::Create(const BufferCreateInfo& createInfo)
    {
        FL_ASSERT(Context::BackendType() != BackendType::None, "Must initialize Context before creating a Buffer");

        try
        {
            switch (Context::BackendType())
            {
                case BackendType::Vulkan: { return std::make_shared<Vulkan::Buffer>(createInfo); }
            }
        }
        catch (const std::exception& e) {}

        FL_ASSERT(false, "Failed to create Buffer");
        return nullptr;
    }
}