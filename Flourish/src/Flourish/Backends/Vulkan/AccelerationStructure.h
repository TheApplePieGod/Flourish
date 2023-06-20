#pragma once

#include "Flourish/Api/AccelerationStructure.h"
#include "Flourish/Backends/Vulkan/Util/Common.h"

namespace Flourish::Vulkan
{
    class AccelerationStructure : public Flourish::AccelerationStructure
    {
    public:
        AccelerationStructure(const AccelerationStructureCreateInfo& createInfo);
        ~AccelerationStructure() override;

        void Build(void* vertexData, u32 vertexStride, u32 vertexCount, u32* indexData, u32 indexCount) override;
        void Build(Flourish::Buffer* vertexBuffer, Flourish::Buffer* indexBuffer) override;

    private:
        void BuildInternal(
            const VkAccelerationStructureGeometryKHR& geom,
            const VkAccelerationStructureBuildRangeInfoKHR* rangeInfo
        );

    };
}
