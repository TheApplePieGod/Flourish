#include "flpch.h"
#include "Aftermath.h"

#ifdef FL_USE_AFTERMATH

#include <GFSDK_Aftermath.h>
#include <GFSDK_Aftermath_GpuCrashDump.h>
#include <GFSDK_Aftermath_GpuCrashDumpDecoding.h>

#define FL_AFTERMATH_CHECK_RESULT(func, msg) \
{ \
    auto res = func; \
    if (!GFSDK_Aftermath_SUCCEED(res)) \
    { FL_LOG_ERROR(msg " failed: %x", res); } \
}

namespace Flourish::Vulkan
{
    void GpuCrashDumpCallback(const void* gpuCrashDump, const uint32_t gpuCrashDumpSize, void* pUserData)
    {
        FL_LOG_DEBUG("GPU crash dump callback called");

        /*
        GFSDK_Aftermath_GpuCrashDump_Decoder decoder = {};
        FL_AFTERMATH_CHECK_RESULT(GFSDK_Aftermath_GpuCrashDump_CreateDecoder(
            GFSDK_Aftermath_Version_API,
            gpuCrashDump,
            gpuCrashDumpSize,
            &decoder
        ), "Aftermath create dump decoder");

        u32 jsonSize = 0;
        GFSDK_Aftermath_GpuCrashDump_GenerateJSON(
            decoder,
            GFSDK_Aftermath_GpuCrashDumpDecoderFlags_ALL_INFO,
            GFSDK_Aftermath_GpuCrashDumpFormatterFlags_NONE,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            &jsonSize
        );

        std::vector<char> json(jsonSize + 1, 0);
        GFSDK_Aftermath_GpuCrashDump_GetJSON(
            decoder,
            jsonSize,
            json.data()
        );

        GFSDK_Aftermath_GpuCrashDump_DestroyDecoder(decoder);
        */

        static int count = 0;
        const std::string baseFileName = "dump-" + std::to_string(++count);

        // Write the crash dump data to a file using the .nv-gpudmp extension
        // registered with Nsight Graphics.
        const std::string crashDumpFileName = baseFileName + ".nv-gpudmp";
        std::ofstream dumpFile(crashDumpFileName, std::ios::out | std::ios::binary);
        if (dumpFile)
            dumpFile.write((const char*)gpuCrashDump, gpuCrashDumpSize);
    }

    void ShaderDebugInfoCallback(const void* pShaderDebugInfo, const uint32_t shaderDebugInfoSize, void* pUserData)
    {
        FL_LOG_DEBUG("Shader debug info callback called");

        // Get shader debug information identifier
        GFSDK_Aftermath_ShaderDebugInfoIdentifier identifier{};
        FL_AFTERMATH_CHECK_RESULT(GFSDK_Aftermath_GetShaderDebugInfoIdentifier(
            GFSDK_Aftermath_Version_API,
            pShaderDebugInfo,
            shaderDebugInfoSize,
            &identifier
        ), "Aftermath shader debug info");

        // Write to file for later in-depth analysis of crash dumps with Nsight Graphics
        const std::string filePath = "shader-" + std::to_string(identifier.id[0]) + std::to_string(identifier.id[1]) + ".nvdbg";
        std::ofstream debugFile(filePath, std::ios::out | std::ios::binary);
        if (debugFile)
            debugFile.write((const char*)pShaderDebugInfo, shaderDebugInfoSize);
    }

    void CrashDumpDescriptionCallback(PFN_GFSDK_Aftermath_AddGpuCrashDumpDescription addDescription, void* pUserData)
    {

    }

    void Aftermath::Initialize()
    {
        FL_LOG_INFO("Initializing Aftermath");

        FL_AFTERMATH_CHECK_RESULT(GFSDK_Aftermath_EnableGpuCrashDumps(
            GFSDK_Aftermath_Version_API,
            GFSDK_Aftermath_GpuCrashDumpWatchedApiFlags_Vulkan,
            GFSDK_Aftermath_GpuCrashDumpFeatureFlags_DeferDebugInfoCallbacks,
            GpuCrashDumpCallback,
            ShaderDebugInfoCallback,
            nullptr,
            nullptr,
            nullptr
        ), "Aftermath init");
    }

    void Aftermath::Shutdown()
    {
        GFSDK_Aftermath_DisableGpuCrashDumps();
    }
}

#endif
