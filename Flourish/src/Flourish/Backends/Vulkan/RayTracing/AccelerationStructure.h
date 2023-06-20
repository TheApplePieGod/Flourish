#pragma once

#include "Flourish/Api/RayTracing/AccelerationStructure.h"
#include "Flourish/Backends/Vulkan/Util/Common.h"

namespace Flourish::Vulkan
{
    class Buffer;
    class AccelerationStructure : public Flourish::AccelerationStructure
    {
    public:
        AccelerationStructure(const AccelerationStructureCreateInfo& createInfo);
        ~AccelerationStructure() override;

        void Build(Flourish::Buffer* vertexBuffer, Flourish::Buffer* indexBuffer) override;
        void Build(AccelerationStructureInstance* instances, u32 instanceCount) override;

        // TS
        inline VkAccelerationStructureKHR GetAccelStructure() const { return m_AccelStructure; }

    private:
        void BuildInternal(
            const VkAccelerationStructureGeometryKHR& geom,
            const VkAccelerationStructureBuildRangeInfoKHR* rangeInfo
        );

    private:
        VkAccelerationStructureKHR m_AccelStructure = nullptr;
        std::shared_ptr<Buffer> m_AccelBuffer;
        std::shared_ptr<Buffer> m_ScratchBuffer;
    };
}
