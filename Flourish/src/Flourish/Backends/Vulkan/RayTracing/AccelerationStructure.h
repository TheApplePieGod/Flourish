#pragma once

#include "Flourish/Api/RayTracing/AccelerationStructure.h"
#include "Flourish/Backends/Vulkan/Util/Common.h"
#include "Flourish/Backends/Vulkan/Util/Commands.h"

namespace Flourish::Vulkan
{
    class Buffer;
    class AccelerationStructure : public Flourish::AccelerationStructure
    {
    public:
        AccelerationStructure(const AccelerationStructureCreateInfo& createInfo);
        ~AccelerationStructure() override;

        void BuildNode(
            Flourish::Buffer* vertexBuffer,
            Flourish::Buffer* indexBuffer,
            bool update = false
        ) override;
        void BuildScene(
            AccelerationStructureInstance* instances,
            u32 instanceCount,
            bool update = false
        ) override;

        // TS
        inline VkAccelerationStructureKHR GetAccelStructure() const { return m_AccelStructure; }

    private:
        void BuildInternal(
            VkAccelerationStructureBuildGeometryInfoKHR& buildInfo,
            const VkAccelerationStructureBuildRangeInfoKHR* rangeInfo,
            VkCommandBuffer cmdBuf
        );
        void CleanupAccel();
        CommandBufferAllocInfo BeginCommands(VkCommandBuffer* cmdBuf);

    private:
        VkAccelerationStructureKHR m_AccelStructure = nullptr;
        std::shared_ptr<Buffer> m_AccelBuffer;
        std::shared_ptr<Buffer> m_ScratchBuffer;
        u32 m_ScratchAlignment;
    };
}
