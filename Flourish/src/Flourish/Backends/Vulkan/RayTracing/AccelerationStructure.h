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

        void RebuildNode(const AccelerationStructureNodeBuildInfo& buildInfo) override;
        void RebuildScene(const AccelerationStructureSceneBuildInfo& buildInfo) override;

        bool IsBuilt() const override { return m_AccelStructure; }

        void RebuildNodeInternal(const AccelerationStructureNodeBuildInfo& buildInfo, VkCommandBuffer cmdBuf);
        void RebuildSceneInternal(const AccelerationStructureSceneBuildInfo& buildInfo, VkCommandBuffer cmdBuf);

        // TS
        inline VkAccelerationStructureKHR GetAccelStructure() const { return m_AccelStructure; }

    private:
        void BuildInternal(
            VkAccelerationStructureBuildGeometryInfoKHR& buildInfo,
            const VkAccelerationStructureBuildRangeInfoKHR* rangeInfo,
            VkCommandBuffer cmdBuf,
            bool skipBuild
        );
        void CleanupAccel();
        CommandBufferAllocInfo BeginCommands(VkCommandBuffer* cmdBuf);
        void EndCommands(
            const CommandBufferAllocInfo& alloc,
            VkCommandBuffer buf,
            bool async,
            std::function<void()> callback
        );

    private:
        u32 m_WriteIndex = 0;
        VkAccelerationStructureKHR m_AccelStructure = VK_NULL_HANDLE;
        std::shared_ptr<Buffer> m_AccelBuffer;
        std::shared_ptr<Buffer> m_ScratchBuffer;
        std::vector<VkAccelerationStructureInstanceKHR> m_Instances;

        std::shared_ptr<Buffer> m_InstanceBuffer;
    };
}
