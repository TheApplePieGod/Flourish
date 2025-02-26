#include "flpch.h"
#include "RayTracingGroupTable.h"

#include "Flourish/Backends/Vulkan/Buffer.h"
#include "Flourish/Backends/Vulkan/Context.h"
#include "Flourish/Backends/Vulkan/RayTracing/RayTracingPipeline.h"

namespace Flourish::Vulkan
{
    RayTracingGroupTable::RayTracingGroupTable(const RayTracingGroupTableCreateInfo& createInfo)
        : Flourish::RayTracingGroupTable(createInfo)
    {
        m_BaseAlignment = Context::Devices().RayTracingProperties().shaderGroupBaseAlignment;
        m_AlignedHandleSize = FL_ALIGN_UP(
            Context::Devices().RayTracingProperties().shaderGroupHandleSize,
            Context::Devices().RayTracingProperties().shaderGroupHandleAlignment
        );

        VkBufferUsageFlags usageFlags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR;
        BufferCreateInfo bufCreateInfo;
        bufCreateInfo.MemoryType = BufferMemoryType::CPUWrite;
        bufCreateInfo.Stride = 1;
        bufCreateInfo.ExposeGPUAddress = true;

        bufCreateInfo.ElementCount = m_AlignedHandleSize + m_BaseAlignment;
        m_Buffers[(u32)RayTracingShaderGroupType::RayGen] = std::make_shared<Buffer>(bufCreateInfo, usageFlags);

        bufCreateInfo.ElementCount = (createInfo.MaxHitEntries * m_AlignedHandleSize) + m_BaseAlignment;
        m_Buffers[(u32)RayTracingShaderGroupType::Hit] = std::make_shared<Buffer>(bufCreateInfo, usageFlags);

        bufCreateInfo.ElementCount = (createInfo.MaxMissEntries * m_AlignedHandleSize) + m_BaseAlignment;
        m_Buffers[(u32)RayTracingShaderGroupType::Miss] = std::make_shared<Buffer>(bufCreateInfo, usageFlags);

        bufCreateInfo.ElementCount = (createInfo.MaxCallableEntries * m_AlignedHandleSize) + m_BaseAlignment;
        m_Buffers[(u32)RayTracingShaderGroupType::Callable] = std::make_shared<Buffer>(bufCreateInfo, usageFlags);
    }

    RayTracingGroupTable::~RayTracingGroupTable()
    {

    }

    void RayTracingGroupTable::BindRayGenGroup(u32 groupIndex)
    {
        BindInternal(groupIndex, 0, RayTracingShaderGroupType::RayGen);
    }

    void RayTracingGroupTable::BindHitGroup(u32 groupIndex, u32 offset)
    {
        FL_ASSERT(offset < m_Info.MaxHitEntries, "Offset exceeds MaxHitEntries");

        BindInternal(groupIndex, offset, RayTracingShaderGroupType::Hit);
    }

    void RayTracingGroupTable::BindMissGroup(u32 groupIndex, u32 offset)
    {
        FL_ASSERT(offset < m_Info.MaxMissEntries, "Offset exceeds MaxMissEntries");

        BindInternal(groupIndex, offset, RayTracingShaderGroupType::Miss);
    }

    void RayTracingGroupTable::BindCallableGroup(u32 groupIndex, u32 offset)
    {
        FL_ASSERT(offset < m_Info.MaxCallableEntries, "Offset exceeds MaxCallableEntries");

        BindInternal(groupIndex, offset, RayTracingShaderGroupType::Callable);
    }

    VkStridedDeviceAddressRegionKHR RayTracingGroupTable::GetBufferRegion(RayTracingShaderGroupType group)
    {
        Buffer* buffer = GetBuffer(group);
        VkDeviceAddress bufferAddr = (VkDeviceAddress)buffer->GetBufferGPUAddress();
        VkStridedDeviceAddressRegionKHR region{};
        region.deviceAddress = FL_ALIGN_UP(bufferAddr, m_BaseAlignment);
        region.size = buffer->GetAllocatedSize() - m_BaseAlignment;
        region.stride = m_AlignedHandleSize;
        return region;
    }

    void RayTracingGroupTable::BindInternal(u32 groupIndex, u32 offset, RayTracingShaderGroupType group)
    {
        u8* handle = static_cast<RayTracingPipeline*>(m_Info.Pipeline.get())->GetGroupHandle(groupIndex);
        Buffer* buffer = GetBuffer(group);

        // TODO: cache this value?
        VkDeviceAddress bufferAddr = (VkDeviceAddress)buffer->GetBufferGPUAddress();
        u32 padStart = FL_ALIGN_UP(bufferAddr, m_BaseAlignment) - bufferAddr;
        buffer->SetBytes(
            handle,
            Context::Devices().RayTracingProperties().shaderGroupHandleSize,
            padStart + m_AlignedHandleSize * offset
        );
    }
}
