#pragma once

#include "Flourish/Api/FeatureTable.h"

namespace Flourish
{
    enum class BackendType
    {
        None = 0,
        Vulkan,
        Metal
    };

    struct ContextInitializeInfo
    {
        BackendType Backend;
        const char* ApplicationName = "App";
        u32 MajorVersion = 1;
        u32 MinorVersion = 0;
        u32 PatchVersion = 0;
        u32 FrameBufferCount = 2;
        bool UseReversedZBuffer = true;
    };

    class CommandBuffer;
    class Context
    {
    public:
        static void Initialize(const ContextInitializeInfo& initInfo);
        static void Shutdown();
        static void BeginFrame();
        static void EndFrame();

        // TS
        static int SubmitCommandBuffers(const std::vector<std::vector<CommandBuffer*>>& buffers);

        // TS
        inline static BackendType BackendType() { return s_BackendType; }
        inline static u32 FrameBufferCount() { return s_FrameBufferCount; }
        inline static u64 FrameCount() { return s_FrameCount; }
        inline static u32 FrameIndex() { return s_FrameIndex; }
        inline static bool ReversedZBuffer() { return s_ReversedZBuffer; }
        inline static FeatureTable& FeatureTable() { return s_FeatureTable; }
        inline static auto& SubmittedCommandBuffers() { return s_SubmittedCommandBuffers; }
        inline static auto& SubmittedCommandBufferCounts() { return s_SubmittedCommandBufferCounts; }

        inline static constexpr u32 MaxFrameBufferCount = 3;

    private:
        inline static Flourish::BackendType s_BackendType = BackendType::None;
        inline static bool s_ReversedZBuffer = true;
        inline static u32 s_FrameBufferCount = 0;
        inline static u64 s_FrameCount = 0;
        inline static u32 s_FrameIndex = 0;
        inline static Flourish::FeatureTable s_FeatureTable;
        inline static std::vector<std::vector<CommandBuffer*>> s_SubmittedCommandBuffers;
        inline static std::vector<u32> s_SubmittedCommandBufferCounts;
        inline static std::mutex s_SubmittedCommandBuffersLock;
    };
}