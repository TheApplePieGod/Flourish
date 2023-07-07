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
    }

    AccelerationStructure::~AccelerationStructure()
    {
        CleanupAccel();
    }

    void AccelerationStructure::RebuildNode(const AccelerationStructureNodeBuildInfo& buildInfo)
    {
        VkCommandBuffer cmdBuf;
        auto cmdAlloc = BeginCommands(&cmdBuf);

        RebuildNodeInternal(buildInfo, cmdBuf);

        EndCommands(cmdAlloc, cmdBuf, buildInfo.AsyncCompletion, buildInfo.CompletionCallback);
    }

    void AccelerationStructure::RebuildScene(const AccelerationStructureSceneBuildInfo& buildInfo)
    {
        VkCommandBuffer cmdBuf;
        auto cmdAlloc = BeginCommands(&cmdBuf);

        RebuildSceneInternal(buildInfo, cmdBuf);

        EndCommands(cmdAlloc, cmdBuf, buildInfo.AsyncCompletion, buildInfo.CompletionCallback);
    }

    void AccelerationStructure::RebuildNodeInternal(const AccelerationStructureNodeBuildInfo& buildInfo, VkCommandBuffer cmdBuf)
    {
        FL_ASSERT(
            m_Info.Type == Flourish::AccelerationStructureType::Node,
            "Type must be Node to call BuildNode"
        );

        if (buildInfo.AABBBuffer)
        {
            FL_ASSERT(
                !buildInfo.VertexBuffer && !buildInfo.IndexBuffer,
                "Acceleration structure rebuild cannot include both vertex/index buffers and AABB buffer"
            );
            FL_ASSERT(
                buildInfo.AABBBuffer->CanCreateAccelerationStructure(),
                "AABB buffer must be created with CanCreateAccelerationStructure"
            );
        }
        else
        {
            FL_ASSERT(
                buildInfo.VertexBuffer->CanCreateAccelerationStructure() && buildInfo.IndexBuffer->CanCreateAccelerationStructure(),
                "Vertex and index buffers must be created with CanCreateAccelerationStructure"
            );
        }

        // TODO HERE
        // - triangle topology only
        // - require indices?
        // - vertex format
        // - index format
        // - transform data

        VkAccelerationStructureGeometryKHR asGeom{};
        asGeom.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        asGeom.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;

        VkAccelerationStructureBuildRangeInfoKHR rangeInfo;
        rangeInfo.firstVertex = 0;
        rangeInfo.primitiveOffset = 0;
        rangeInfo.transformOffset = 0;

        VkAccelerationStructureGeometryTrianglesDataKHR triangleGeom{};
        VkAccelerationStructureGeometryAabbsDataKHR aabbGeom{};
        if (buildInfo.AABBBuffer)
        {
            FL_ASSERT(
                buildInfo.AABBBuffer->GetStride() == 24,
                "AABB buffer must have exactly 24 byte stride"
            );

            aabbGeom.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_AABBS_DATA_KHR;
            aabbGeom.data.deviceAddress = (VkDeviceAddress)buildInfo.AABBBuffer->GetBufferGPUAddress();
            aabbGeom.stride = 24; // min & max

            asGeom.geometryType = VK_GEOMETRY_TYPE_AABBS_KHR;
            asGeom.geometry.aabbs = aabbGeom;

            rangeInfo.primitiveCount = buildInfo.AABBBuffer->GetAllocatedCount();
        }
        else
        {
            VkDeviceAddress vertexAddress = (VkDeviceAddress)buildInfo.VertexBuffer->GetBufferGPUAddress();
            VkDeviceAddress indexAddress = (VkDeviceAddress)buildInfo.IndexBuffer->GetBufferGPUAddress();

            triangleGeom.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
            triangleGeom.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
            triangleGeom.vertexData.deviceAddress = vertexAddress;
            triangleGeom.vertexStride = buildInfo.VertexBuffer->GetStride();
            triangleGeom.indexType = VK_INDEX_TYPE_UINT32;
            triangleGeom.indexData.deviceAddress = indexAddress;
            triangleGeom.maxVertex = buildInfo.VertexBuffer->GetAllocatedCount();

            asGeom.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
            asGeom.geometry.triangles = triangleGeom;

            u32 maxPrimitiveCount = buildInfo.IndexBuffer->GetAllocatedCount() / 3;
            rangeInfo.primitiveCount = maxPrimitiveCount;
        }

        VkAccelerationStructureBuildGeometryInfoKHR buildInfoVk{};
        buildInfoVk.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        buildInfoVk.geometryCount = 1;
        buildInfoVk.pGeometries = &asGeom;
        buildInfoVk.mode = buildInfo.TryUpdate ? VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR : VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;

        BuildInternal(buildInfoVk, &rangeInfo, cmdBuf);
    }

    void AccelerationStructure::RebuildSceneInternal(const AccelerationStructureSceneBuildInfo& buildInfo, VkCommandBuffer cmdBuf)
    {
        FL_ASSERT(
            m_Info.Type == Flourish::AccelerationStructureType::Scene,
            "Type must be Scene to call BuildNode"
        );

        // Populate instances
        m_Instances.clear();
        m_Instances.reserve(buildInfo.InstanceCount);
        VkAccelerationStructureInstanceKHR instance{};
        instance.mask = 0xFF;
        instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
        instance.instanceShaderBindingTableRecordOffset = 0; // Revisit this / parameterize?
        VkAccelerationStructureDeviceAddressInfoKHR accelInfo{};
        accelInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
        for (u32 i = 0; i < buildInfo.InstanceCount; i++)
        {
            // Convert 4x4 col -> 3x4 row major
            // SIMD?
            u32 srcIdx = 0;
            for (u32 dstIdx = 0; dstIdx < 12; dstIdx++)
            {
                if ((srcIdx + 1) % 4 == 0)
                    srcIdx++;
                instance.transform.matrix[dstIdx % 3][dstIdx / 3] = buildInfo.Instances[i].TransformMatrix[srcIdx++];
            }
            accelInfo.accelerationStructure = static_cast<const AccelerationStructure*>(buildInfo.Instances[i].Parent)->GetAccelStructure();
            instance.accelerationStructureReference = vkGetAccelerationStructureDeviceAddressKHR(Context::Devices().Device(), &accelInfo);
            instance.instanceCustomIndex = buildInfo.Instances[i].CustomIndex;
            m_Instances.emplace_back(instance);
        }


        auto& instanceBuffer = m_InstanceBuffers[m_WriteIndex];
        if (buildInfo.InstanceCount > 0)
        {
            m_WriteIndex = (m_WriteIndex + 1) % Flourish::Context::FrameBufferCount();
            if (!instanceBuffer || instanceBuffer->GetAllocatedCount() < buildInfo.InstanceCount)
            {
                BufferCreateInfo ibCreateInfo;
                ibCreateInfo.Usage = BufferUsageType::DynamicOneFrame;
                ibCreateInfo.ElementCount = buildInfo.InstanceCount;
                ibCreateInfo.Stride = sizeof(VkAccelerationStructureInstanceKHR);
                ibCreateInfo.InitialData = m_Instances.data();
                ibCreateInfo.InitialDataSize = sizeof(VkAccelerationStructureInstanceKHR) * buildInfo.InstanceCount;
                ibCreateInfo.ExposeGPUAddress = true;
                instanceBuffer = std::make_shared<Buffer>(
                    ibCreateInfo,
                    VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                    Buffer::MemoryDirection::CPUToGPU,
                    cmdBuf,
                    true
                );
            }
            else
            {
                instanceBuffer->SetElements(m_Instances.data(), m_Instances.size(), 0);
                instanceBuffer->FlushInternal(cmdBuf, false);
            }

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

            // Ensure all instance writes are complete
            barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
            barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
            vkCmdPipelineBarrier(
                cmdBuf,
                VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                0,
                1, &barrier,
                0, nullptr,
                0, nullptr
            );
        }

        VkAccelerationStructureGeometryKHR topGeom{};
        topGeom.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        topGeom.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
        topGeom.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
        if (buildInfo.InstanceCount > 0)
            topGeom.geometry.instances.data.deviceAddress = (VkDeviceAddress)instanceBuffer->GetBufferGPUAddress();

        VkAccelerationStructureBuildGeometryInfoKHR buildInfoVk{};
        buildInfoVk.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        buildInfoVk.geometryCount = 1;
        buildInfoVk.pGeometries = &topGeom;
        buildInfoVk.mode = buildInfo.TryUpdate ? VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR : VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;

        VkAccelerationStructureBuildRangeInfoKHR rangeInfo;
        rangeInfo.firstVertex = 0;
        rangeInfo.primitiveCount = buildInfo.InstanceCount;
        rangeInfo.primitiveOffset = 0;
        rangeInfo.transformOffset = 0;

        BuildInternal(buildInfoVk, &rangeInfo, cmdBuf);

        if (m_Info.BuildFrequency != AccelerationStructureBuildFrequency::Often)
            instanceBuffer.reset();
    }

    void AccelerationStructure::BuildInternal(
        VkAccelerationStructureBuildGeometryInfoKHR& buildInfo,
        const VkAccelerationStructureBuildRangeInfoKHR* rangeInfo,
        VkCommandBuffer cmdBuf
    )
    {
        FL_ASSERT(
            !IsBuilt() || m_Info.BuildFrequency != AccelerationStructureBuildFrequency::Once,
            "Cannot rebuild acceleration structure that has build frequency once"
        );

        // Force build if this is the first time
        if (!IsBuilt())
            buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        bool isUpdating = buildInfo.mode == VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR;

        FL_ASSERT(
            !isUpdating || m_Info.AllowUpdating,
            "AllowUpdating must be true to update AccelerationStructure"
        );

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
        VkAccelerationStructureBuildSizesInfoKHR buildSize{};
        buildSize.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
        vkGetAccelerationStructureBuildSizesKHR(
            Context::Devices().Device(),
            VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
            &buildInfo,
            &rangeInfo->primitiveCount,
            &buildSize
        );

        // Allocate scratch buffer
        VkDeviceSize alignment = Context::Devices().AccelStructureProperties().minAccelerationStructureScratchOffsetAlignment;
        u32 scratchSize = alignment;
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
            scratchCreateInfo.ExposeGPUAddress = true;
            m_ScratchBuffer = std::make_shared<Buffer>(
                scratchCreateInfo,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT ,
                Buffer::MemoryDirection::CPUToGPU
            );
        }
        VkDeviceAddress scratchAddress = FL_ALIGN_UP(
            (VkDeviceAddress)m_ScratchBuffer->GetBufferGPUAddress(),
            alignment
        );

        VkAccelerationStructureCreateInfoKHR accCreateInfo{};
        accCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
        accCreateInfo.size = buildSize.accelerationStructureSize;
        accCreateInfo.type = buildInfo.type;

        // Allocate buffer to store acceleration structure
        BufferCreateInfo abCreateInfo;
        abCreateInfo.Usage = BufferUsageType::Static;
        abCreateInfo.ElementCount = 1;
        abCreateInfo.Stride = buildSize.accelerationStructureSize;

        // Set the src BEFORE we potentially cleanup in case we need to allocate
        // a larger buffer and update
        buildInfo.srcAccelerationStructure = m_AccelStructure;

        if (!m_AccelBuffer || m_AccelBuffer->GetAllocatedSize() < buildSize.accelerationStructureSize)
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
        buildInfo.dstAccelerationStructure = m_AccelStructure;
        buildInfo.scratchData.deviceAddress = scratchAddress;

        vkCmdBuildAccelerationStructuresKHR(cmdBuf, 1, &buildInfo, &rangeInfo);

        VkMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
        barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
        vkCmdPipelineBarrier(
            cmdBuf,
            VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
            VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
            0,
            1, &barrier,
            0, nullptr,
            0, nullptr
        );

        // TODO: revisit this. Ideally, we free this memory when the update is done, but
        // the way it is set up now, we would have to rely on the assumption that the finalizer queue
        // happens way after the work is complete. This is probably fine, but still sus.
        // We can consider increasing finalizer values by a significant amount.
        // We should probably also look into when freeing memory is ideal
        if (m_Info.BuildFrequency != AccelerationStructureBuildFrequency::Often)
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

    void AccelerationStructure::EndCommands(const CommandBufferAllocInfo& alloc, VkCommandBuffer buf, bool async, std::function<void()> callback)
    {
        vkEndCommandBuffer(buf);

        if (!async)
        {
            Context::Queues().ExecuteCommand(GPUWorkloadType::Compute, buf); //, "AccelerationStructure execute");
            Context::Commands().FreeBuffer(alloc, buf);
            return;
        }

        Context::Queues().PushCommand(GPUWorkloadType::Compute, buf, [buf, alloc, callback]()
        {
            Context::Commands().FreeBuffer(alloc, buf);
            if (callback)
                callback();
        }); //, "AccelerationStructure command free");
    }
}
