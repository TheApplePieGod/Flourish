#include "flpch.h"
#include "DebugCheckpoints.h"

#include "Flourish/Backends/Vulkan/Context.h"

namespace Flourish::Vulkan
{
    u32 DebugCheckpoints::PushCheckpoint(VkCommandBuffer buffer, Checkpoint checkpoint)
    {
        s_Mutex.lock();
        u32 newId = ++s_MinId;
        s_Checkpoints[newId] = checkpoint;
        vkCmdSetCheckpointNV(buffer, (void*)newId);
        s_Mutex.unlock();
        return newId;
    }

    void DebugCheckpoints::LogCheckpoint(u32 topIndex)
    {
        s_Mutex.lock();
        while (topIndex != 0 && s_Checkpoints.find(topIndex) != s_Checkpoints.end())
        {
            auto& checkpoint = s_Checkpoints[topIndex];
            FL_LOG_WARN("    Id: %d, Msg: %s", topIndex, checkpoint.Data);
            topIndex = checkpoint.LastCheckpointId;            
        }
        s_Mutex.unlock();
    }

    void LogCheckpointData(VkCheckpointDataNV* data, u32 count)
    {
        for (u32 i = 0; i < count; i++)
        {
            if (data[i].stage == VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT)
            {
                FL_LOG_WARN("Top of pipe checkpoints:");
                DebugCheckpoints::LogCheckpoint((u32)data[i].pCheckpointMarker);
            }
            else if (data[i].stage == VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT)
            {
                FL_LOG_WARN("Bottom of pipe checkpoints:");
                DebugCheckpoints::LogCheckpoint((u32)data[i].pCheckpointMarker);
            }
        }
    }

    void DebugCheckpoints::LogCheckpoints()
    {
        for (u32 frame = 0; frame < Flourish::Context::FrameBufferCount(); frame++)
        {
            u32 count = 0;
            vkGetQueueCheckpointDataNV(Context::Queues().Queue(GPUWorkloadType::Graphics, frame), &count, nullptr);
            std::vector<VkCheckpointDataNV> checkpoints(count, { VK_STRUCTURE_TYPE_CHECKPOINT_DATA_NV });
            vkGetQueueCheckpointDataNV(Context::Queues().Queue(GPUWorkloadType::Graphics, frame), &count, checkpoints.data());
            
            FL_LOG_WARN("Dumping graphics queue checkpoints (frame %d)\n---------------", frame);
            LogCheckpointData(checkpoints.data(), count);
            FL_LOG_WARN("---------------");

            vkGetQueueCheckpointDataNV(Context::Queues().Queue(GPUWorkloadType::Compute, frame), &count, nullptr);
            checkpoints = std::vector<VkCheckpointDataNV>(count, { VK_STRUCTURE_TYPE_CHECKPOINT_DATA_NV });
            vkGetQueueCheckpointDataNV(Context::Queues().Queue(GPUWorkloadType::Compute, frame), &count, checkpoints.data());

            FL_LOG_WARN("Dumping compute queue checkpoints (frame %d)\n---------------", frame);
            LogCheckpointData(checkpoints.data(), count);
            FL_LOG_WARN("---------------");

            vkGetQueueCheckpointDataNV(Context::Queues().Queue(GPUWorkloadType::Transfer, frame), &count, nullptr);
            checkpoints = std::vector<VkCheckpointDataNV>(count, { VK_STRUCTURE_TYPE_CHECKPOINT_DATA_NV });
            vkGetQueueCheckpointDataNV(Context::Queues().Queue(GPUWorkloadType::Transfer, frame), &count, checkpoints.data());

            FL_LOG_WARN("Dumping transfer queue checkpoints (frame %d)\n---------------", frame);
            LogCheckpointData(checkpoints.data(), count);
            FL_LOG_WARN("---------------");
        }
    }

    void DebugCheckpoints::FreeCheckpoints(u32 topIndex)
    {
        s_Mutex.lock();
        while (topIndex != 0 && s_Checkpoints.find(topIndex) != s_Checkpoints.end())
        {
            u32 temp = s_Checkpoints[topIndex].LastCheckpointId;
            s_Checkpoints.erase(topIndex);
            topIndex = temp;
        }
        s_Mutex.unlock();
    }
}