#pragma once

#include "Flourish/Backends/Vulkan/Util/Common.h"

namespace Flourish::Vulkan
{
    struct Checkpoint
    {
        const char* Data;
        u32 LastCheckpointId = 0;
    };
    
    class DebugCheckpoints
    {
    public:
        static u32 PushCheckpoint(VkCommandBuffer buffer, Checkpoint checkpoint);
        static void LogCheckpoint(u32 topIndex);
        static void LogCheckpoints();
        static void FreeCheckpoints(u32 topIndex);
        
    private:
        inline static std::mutex s_Mutex;
        inline static std::unordered_map<u32, Checkpoint> s_Checkpoints;
        inline static u32 s_MinId = 0;
    };
}
