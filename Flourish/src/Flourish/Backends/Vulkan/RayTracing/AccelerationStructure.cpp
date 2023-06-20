#include "flpch.h"
#include "AccelerationStructure.h"

#include "Flourish/Backends/Vulkan/Context.h"
#include "Flourish/Backends/Vulkan/Buffer.h"

namespace Flourish::Vulkan
{
    AccelerationStructure::AccelerationStructure(const AccelerationStructureCreateInfo& createInfo)
        : Flourish::AccelerationStructure(createInfo)
    {
        FL_ASSERT(
            Flourish::Context::FeatureTable().RayTracing,
            "RayTracing feature must be enabled and supported to use AccelerationStructures"
        );

        // TODO: this should be cached somehwere
        VkPhysicalDeviceAccelerationStructurePropertiesKHR accelProps{};
        accelProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR;
        VkPhysicalDeviceProperties2 devProps{};
        devProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        devProps.pNext = &accelProps;
        vkGetPhysicalDeviceProperties2(Context::Devices().PhysicalDevice(), &devProps);
        m_ScratchAlignment = accelProps.minAccelerationStructureScratchOffsetAlignment;
    }

    AccelerationStructure::~AccelerationStructure()
    {
        CleanupAccel();
    }

    // TODO: build directly from vertex and index data?
    // that would change BUILD_TYPE from device to host
    void AccelerationStructure::BuildNode(Flourish::Buffer* vertexBuffer, Flourish::Buffer* indexBuffer, bool update)
    {
        FL_ASSERT(
            vertexBuffer->CanCreateAccelerationStructure() && indexBuffer->CanCreateAccelerationStructure(),
            "Vertex and index buffers must be created with CanCreateAccelerationStructure"
        );

        FL_ASSERT(
            m_Info.Type == Flourish::AccelerationStructureType::Node,
            "Type must be Node to call BuildNode"
        );

        FL_ASSERT(
            !update || m_Info.AllowUpdating,
            "AllowUpdating must be true to update node"
        );

        // TODO HERE
        // - triangle topology only
        // - require indices?
        // - vertex format
        // - index format
        // - transform data

        VkDeviceAddress vertexAddress = static_cast<Buffer*>(vertexBuffer)->GetBufferDeviceAddress();
        VkDeviceAddress indexAddress = static_cast<Buffer*>(indexBuffer)->GetBufferDeviceAddress();

        uint32_t maxPrimitiveCount = indexBuffer->GetAllocatedCount() / 3;
        VkAccelerationStructureGeometryTrianglesDataKHR triangleGeom{};
        triangleGeom.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
        triangleGeom.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
        triangleGeom.vertexData.deviceAddress = vertexAddress;
        triangleGeom.vertexStride = vertexBuffer->GetStride();
        triangleGeom.indexType = VK_INDEX_TYPE_UINT32;
        triangleGeom.indexData.deviceAddress = indexAddress;
        triangleGeom.maxVertex = vertexBuffer->GetAllocatedCount();

        VkAccelerationStructureGeometryKHR asGeom{};
        asGeom.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        asGeom.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
        asGeom.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
        asGeom.geometry.triangles = triangleGeom;

        VkAccelerationStructureBuildRangeInfoKHR rangeInfo;
        rangeInfo.firstVertex = 0;
        rangeInfo.primitiveCount = maxPrimitiveCount;
        rangeInfo.primitiveOffset = 0;
        rangeInfo.transformOffset = 0;

        VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
        buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        buildInfo.geometryCount = 1;
        buildInfo.pGeometries = &asGeom;
        buildInfo.mode = update ? VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR : VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;

        VkCommandBuffer cmdBuf;
        auto cmdAlloc = BeginCommands(&cmdBuf);

        BuildInternal(buildInfo, &rangeInfo, cmdBuf);

        vkEndCommandBuffer(cmdBuf);
        Context::Queues().ExecuteCommand(GPUWorkloadType::Compute, cmdBuf);

        Context::Commands().FreeBuffer(cmdAlloc, cmdBuf);
    }

    void AccelerationStructure::BuildScene(AccelerationStructureInstance* instances, u32 instanceCount, bool update)
    {
        FL_ASSERT(
            m_Info.Type == Flourish::AccelerationStructureType::Scene,
            "Type must be Scene to call BuildNode"
        );

        FL_ASSERT(
            !update || m_Info.AllowUpdating,
            "AllowUpdating must be true to update scene"
        );

        // Populate instances
        std::vector<VkAccelerationStructureInstanceKHR> vkInstances;
        vkInstances.reserve(instanceCount);
        VkAccelerationStructureInstanceKHR instance{};
        VkAccelerationStructureDeviceAddressInfoKHR accelInfo{};
        accelInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
        for (u32 i = 0; i < instanceCount; i++)
        {
            // Convert 4x4 col -> 3x4 row major
            // SIMD?
            u32 srcIdx = 0;
            for (u32 dstIdx = 0; dstIdx < 12; dstIdx++)
            {
                if ((srcIdx + 1) % 4 == 0)
                    srcIdx++;
                instance.transform.matrix[dstIdx % 3][dstIdx / 3] = instances[i].TransformMatrix[srcIdx++];
            }
            accelInfo.accelerationStructure = static_cast<const AccelerationStructure*>(instances[i].Parent)->GetAccelStructure();
            instance.accelerationStructureReference = vkGetAccelerationStructureDeviceAddressKHR(Context::Devices().Device(), &accelInfo);
            vkInstances.emplace_back(instance);
        }

        VkCommandBuffer cmdBuf;
        auto cmdAlloc = BeginCommands(&cmdBuf);

        BufferCreateInfo ibCreateInfo;
        ibCreateInfo.Usage = BufferUsageType::Static;
        ibCreateInfo.ElementCount = instanceCount;
        ibCreateInfo.Stride = sizeof(VkAccelerationStructureInstanceKHR);
        ibCreateInfo.InitialData = vkInstances.data();
        ibCreateInfo.InitialDataSize = sizeof(VkAccelerationStructureInstanceKHR) * instanceCount;
        Buffer instanceBuf = Buffer(
            ibCreateInfo,
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            Buffer::MemoryDirection::CPUToGPU,
            cmdBuf
        );

        // Ensure data is uploaded before performing build
        VkMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
        vkCmdPipelineBarrier(
            cmdBuf,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
            0,
            1, &barrier,
            0, nullptr,
            0, nullptr
        );

        VkAccelerationStructureGeometryInstancesDataKHR instanceGeom{};
        instanceGeom.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
        instanceGeom.data.deviceAddress = instanceBuf.GetBufferDeviceAddress();

        VkAccelerationStructureGeometryKHR topGeom{};
        topGeom.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        topGeom.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
        topGeom.geometry.instances = instanceGeom;

        // Get build size
        VkAccelerationStructureBuildGeometryInfoKHR buildInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
        buildInfo.geometryCount = 1;
        buildInfo.pGeometries = &topGeom;
        buildInfo.mode = update ? VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR : VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;

        VkAccelerationStructureBuildRangeInfoKHR rangeInfo;
        rangeInfo.firstVertex = 0;
        rangeInfo.primitiveCount = instanceCount;
        rangeInfo.primitiveOffset = 0;
        rangeInfo.transformOffset = 0;

        BuildInternal(buildInfo, &rangeInfo, cmdBuf);

        vkEndCommandBuffer(cmdBuf);
        Context::Queues().ExecuteCommand(GPUWorkloadType::Compute, cmdBuf);

        Context::Commands().FreeBuffer(cmdAlloc, cmdBuf);
    }

    void AccelerationStructure::BuildInternal(
        VkAccelerationStructureBuildGeometryInfoKHR& buildInfo,
        const VkAccelerationStructureBuildRangeInfoKHR* rangeInfo,
        VkCommandBuffer cmdBuf
    )
    {
        // Force build if this is the first time
        if (!m_AccelStructure)
            buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        bool isUpdating = buildInfo.mode == VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR;

        buildInfo.type = Common::ConvertAccelerationStructureType(m_Info.Type);
        buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        buildInfo.flags = Common::ConvertAccelerationStructurePerformanceType(m_Info.PerformancePreference);
            //VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR |
        if (m_Info.AllowUpdating)
            buildInfo.flags |= VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;

        // TODO HERE:
        // - VK_BUILD_MODE_UPDATE
        // - Fast trace vs fast build

        // Finding sizes to create acceleration structures and scratch
        VkAccelerationStructureBuildSizesInfoKHR buildSize;
        buildSize.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
        vkGetAccelerationStructureBuildSizesKHR(
            Context::Devices().Device(),
            VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
            &buildInfo,
            &rangeInfo->primitiveCount,
            &buildSize
        );

        // Allocate scratch buffer
        u32 scratchSize = m_ScratchAlignment;
        if (isUpdating)
            scratchSize += buildSize.updateScratchSize;
        else
            scratchSize += buildSize.buildScratchSize;
        if (!m_ScratchBuffer || m_ScratchBuffer->GetAllocatedSize() < scratchSize)
        {
            BufferCreateInfo scratchCreateInfo;
            scratchCreateInfo.Usage = BufferUsageType::Static;
            scratchCreateInfo.ElementCount = 1;
            scratchCreateInfo.Stride = scratchSize;
            m_ScratchBuffer = std::make_shared<Buffer>(
                scratchCreateInfo,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                Buffer::MemoryDirection::CPUToGPU
            );
        }
        VkDeviceAddress scratchAddress = m_ScratchBuffer->GetBufferDeviceAddress();
        scratchAddress += (m_ScratchAlignment - (scratchAddress % m_ScratchAlignment));

        VkAccelerationStructureCreateInfoKHR accCreateInfo{};
        accCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
        accCreateInfo.size = buildSize.accelerationStructureSize;
        accCreateInfo.type = buildInfo.type;

        // Allocate buffer to store acceleration structure
        BufferCreateInfo abCreateInfo;
        abCreateInfo.Usage = BufferUsageType::Static;
        abCreateInfo.ElementCount = 1;
        abCreateInfo.Stride = buildSize.accelerationStructureSize;

        // We always allocate a new buffer on build since the old buffer and structure will
        // be cleaned up. TODO: could alternate between buffers to store
        if (!isUpdating)
        {
            CleanupAccel();

            m_AccelBuffer = std::make_shared<Buffer>(
                abCreateInfo,
                VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                Buffer::MemoryDirection::CPUToGPU
            );

            accCreateInfo.buffer = m_AccelBuffer->GetBuffer();
            vkCreateAccelerationStructureKHR(Context::Devices().Device(), &accCreateInfo, nullptr, &m_AccelStructure);
        }

        // Finalize build info
        buildInfo.srcAccelerationStructure = m_AccelStructure;
        buildInfo.dstAccelerationStructure = m_AccelStructure;
        buildInfo.scratchData.deviceAddress = scratchAddress;

        vkCmdBuildAccelerationStructuresKHR(cmdBuf, 1, &buildInfo, &rangeInfo);

        // Cleanup scratch buffer for builds since the update scratch is usually
        // smaller
        if (!isUpdating)
            m_ScratchBuffer.reset();
    }

    void AccelerationStructure::CleanupAccel()
    {
        if (!m_AccelStructure)
            return;

        auto accelStructure = m_AccelStructure;
        Context::FinalizerQueue().Push([=]()
        {
            vkDestroyAccelerationStructureKHR(Context::Devices().Device(), accelStructure, nullptr);
        });
    }

    CommandBufferAllocInfo AccelerationStructure::BeginCommands(VkCommandBuffer* cmdBuf)
    {
        auto commandAlloc = Context::Commands().AllocateBuffers(
            GPUWorkloadType::Compute,
            false,
            cmdBuf, 1,
            true
        );

        VkCommandBufferBeginInfo cmdBeginInfo{};
        cmdBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        cmdBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(*cmdBuf, &cmdBeginInfo);
        
        return commandAlloc;
    }
}
