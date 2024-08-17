#pragma once

#include "Flourish/Api/FeatureTable.h"

namespace Flourish
{
    using ReadFileFn = std::function<unsigned char*(std::string_view, u32&)>;

    enum class BackendType
    {
        None = 0,
        Vulkan,
        Metal
    };

    struct MemoryStatistics
    {
        // Manually allocated objects (i.e. buffers, textures)
        u32 AllocationCount;
        u64 AllocationTotalSize;

        // Pre-allocated block memory
        u32 BlockCount;
        u64 BlockTotalSize;

        // Total reported GPU memory
        u64 TotalAvailable;

        std::string ToString() const;
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
        FeatureTable RequestedFeatures;

        // Custom file read handler. Defaults to standard std::ifstream
        ReadFileFn ReadFile = nullptr;
    };

    class RenderGraph;
    class RenderContext;
    class Context
    {
    public:
        static void Initialize(const ContextInitializeInfo& initInfo);
        static void Shutdown(std::function<void()> finalizer = nullptr);
        static void BeginFrame();
        static void EndFrame();

        // TS
        static void PushFrameRenderGraph(RenderGraph* graph);
        static void PushFrameRenderContext(RenderContext* context);
        static void PushRenderGraph(RenderGraph* graph, std::function<void()> callback = nullptr);
        static void ExecuteRenderGraph(RenderGraph* graph);

        // TS
        static MemoryStatistics ComputeMemoryStatistics();

        // TS
        inline static BackendType BackendType() { return s_BackendType; }
        inline static u32 FrameBufferCount() { return s_FrameBufferCount; }
        inline static u64 FrameCount() { return s_FrameCount; }
        inline static u32 FrameIndex() { return s_FrameIndex; }
        inline static u32 LastFrameIndex() { return s_LastFrameIndex; }
        inline static bool ReversedZBuffer() { return s_ReversedZBuffer; }
        inline static FeatureTable& FeatureTable() { return s_FeatureTable; }
        inline static const auto& ReadFile() { return s_ReadFile; }
        inline static const auto& FrameGraphSubmissions() { return s_GraphSubmissions; }
        inline static const auto& FrameContextSubmissions() { return s_ContextSubmissions; }
        inline static u64 GetNextId() { return s_IdCounter++; }

        inline static constexpr u32 MaxFrameBufferCount = 3;
        
    private:
        inline static Flourish::BackendType s_BackendType = BackendType::None;
        inline static bool s_ReversedZBuffer = true;
        inline static u32 s_FrameBufferCount = 0;
        inline static u64 s_FrameCount = 1;
        inline static u32 s_FrameIndex = 0;
        inline static u32 s_LastFrameIndex = 0;
        inline static Flourish::FeatureTable s_FeatureTable;
        inline static std::vector<RenderGraph*> s_GraphSubmissions;
        inline static std::vector<RenderContext*> s_ContextSubmissions;
        inline static std::mutex s_FrameMutex;
        inline static std::atomic<u64> s_IdCounter = { 1 };
        inline static ReadFileFn s_ReadFile;
    };
}
