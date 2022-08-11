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
        inline static BackendType GetBackendType() { return s_BackendType; }
        inline static u32 GetFrameBufferCount() { return s_FrameBufferCount; }

        inline static constexpr u32 MaxFrameBufferCount = 3;

    private:
        inline static BackendType s_BackendType = BackendType::None;
        inline static u32 s_FrameBufferCount = 0;
        inline static std::unordered_set<std::thread::id> s_RegisteredThreads;
        inline static std::mutex s_RegisteredThreadsLock;
    };
}