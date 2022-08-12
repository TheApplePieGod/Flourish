#pragma once

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

    class Context
    {
    public:
        static void Initialize(const ContextInitializeInfo& initInfo);
        static void Shutdown();

        // TS
        static bool IsThreadRegistered(std::thread::id thread = std::this_thread::get_id());
        static void RegisterThread();
        static void UnregisterThread();

        // TS
        inline static BackendType BackendType() { return s_BackendType; }
        inline static u32 FrameBufferCount() { return s_FrameBufferCount; }
        inline static u64 FrameCount() { return s_FrameBufferCount; }
        inline static bool ReversedZBuffer() { return s_ReversedZBuffer; }

        inline static constexpr u32 MaxFrameBufferCount = 3;

    private:
        inline static Flourish::BackendType s_BackendType = BackendType::None;
        inline static bool s_ReversedZBuffer = true;
        inline static u32 s_FrameBufferCount = 0;
        inline static u64 s_FrameCount = 0;
        inline static std::unordered_set<std::thread::id> s_RegisteredThreads;
        inline static std::mutex s_RegisteredThreadsLock;
    };
}