#pragma once

#include "Flourish/Api/RayTracing/RayTracingGroupTable.h"
#include "Flourish/Api/RayTracing/RayTracingPipeline.h"
#include "Flourish/Backends/Vulkan/Util/Common.h"

namespace Flourish::Vulkan
{
    class Buffer;
    class RayTracingGroupTable : public Flourish::RayTracingGroupTable
    {
    public:
        RayTracingGroupTable(const RayTracingGroupTableCreateInfo& createInfo);
        ~RayTracingGroupTable() override;

        void BindRayGenGroup(u32 groupIndex) override;
        void BindHitGroup(u32 groupIndex, u32 offset) override;
        void BindMissGroup(u32 groupIndex, u32 offset) override;
        void BindCallableGroup(u32 groupIndex, u32 offset) override;

        // TS
        inline Buffer* GetBuffer(RayTracingShaderGroupType group) const { return m_Buffers[(u32)group].get(); }

    private:
        void BindInternal(u32 groupIndex, u32 offset, RayTracingShaderGroupType group);

    private:
        u32 m_AlignedHandleSize;
        u32 m_BaseAlignment;
        std::array<std::shared_ptr<Buffer>, 4> m_Buffers;
    };
}
