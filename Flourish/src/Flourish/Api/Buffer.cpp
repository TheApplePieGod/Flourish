#include "flpch.h"
#include "Buffer.h"

#include "Flourish/Api/Context.h"
#include "Flourish/Backends/Vulkan/Buffer.h"

namespace Flourish
{
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

    void Buffer::SetElements(void* data, u32 elementCount, u32 elementOffset)
    {
        FL_ASSERT(elementCount + elementOffset <= m_Info.ElementCount, "Attempting to set data on buffer which is larger than allocated size");
        SetBytes(data, m_Info.Layout.GetStride() * elementCount, m_Info.Layout.GetStride() * elementOffset);
    }

    std::shared_ptr<Buffer> Buffer::Create(const BufferCreateInfo& createInfo)
    {
        FL_ASSERT(Context::BackendType() != BackendType::None, "Must initialize Context before creating a Buffer");

        switch (Context::BackendType())
        {
            case BackendType::Vulkan: { return std::make_shared<Vulkan::Buffer>(createInfo); }
        }

        FL_ASSERT(false, "Buffer not supported for backend");
        return nullptr;
    }
}