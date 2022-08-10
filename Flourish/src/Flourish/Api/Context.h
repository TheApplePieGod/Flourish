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

    struct ContextCreateInfo
    {

    };

    class Context
    {
    public:
        static void Initialize(const ContextInitializeInfo& initInfo);
        static void Shutdown();

        // TS
        static std::shared_ptr<Context> Create(const ContextCreateInfo& createInfo);

        // TS
        inline static BackendType GetBackendType() { return s_BackendType; }
        inline static u32 GetFrameBufferCount() { return s_FrameBufferCount; }

        inline static constexpr u32 MaxFrameBufferCount = 3;

    private:
        inline static BackendType s_BackendType = BackendType::None;
        inline static u32 s_FrameBufferCount = 0;
    };
}