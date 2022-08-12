#include "flpch.h"

#include "Flourish/Api/Context.h"
#include "Flourish/Api/RenderContext.h"
#include "Flourish/Api/CommandBuffer.h"
#include "Flourish/Api/Buffer.h"

#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"

std::shared_ptr<spdlog::logger> logger; 

void Log(Flourish::LogLevel level, const char* message)
{
    logger->log((spdlog::level::level_enum)level, message);
}

int main(int argc, char** argv)
{
    // Initialize logging
    auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    consoleSink->set_pattern("%^[%T] %n: %v%$");
    logger = std::make_shared<spdlog::logger>("FLOURISH", consoleSink);
    logger->set_level(spdlog::level::trace);
    logger->flush_on(spdlog::level::trace);
    spdlog::register_logger(logger);
    Flourish::Logger::SetLogFunction(Log);

    Flourish::ContextInitializeInfo contextInitInfo;
    contextInitInfo.Backend = Flourish::BackendType::Vulkan;
    contextInitInfo.ApplicationName = "FlourishTesting";
    Flourish::Context::Initialize(contextInitInfo);

    {
        Flourish::RenderContextCreateInfo contextCreateInfo;
        contextCreateInfo.Width = 1920;
        contextCreateInfo.Height = 1080;
        #ifdef FL_PLATFORM_WINDOWS
            HINSTANCE instance = GetModuleHandle(NULL);
            WNDCLASS wc{};
            wc.lpfnWndProc = DefWindowProc;
            wc.hInstance = instance;
            wc.lpszClassName = "Window";
            RegisterClass(&wc);
            HWND hwnd = CreateWindow(
                "Window",
                "Flourish",
                WS_OVERLAPPEDWINDOW,
                CW_USEDEFAULT, CW_USEDEFAULT,
                (int)contextCreateInfo.Width, (int)contextCreateInfo.Height,
                NULL,
                NULL,
                instance,
                NULL
            );
            ShowWindow(hwnd, SW_SHOW);

            contextCreateInfo.Instance = instance;
            contextCreateInfo.Window = hwnd;
        #endif
        auto renderContext = Flourish::RenderContext::Create(contextCreateInfo);

        Flourish::CommandBufferCreateInfo cmdCreateInfo;
        cmdCreateInfo.WorkloadType = Flourish::GPUWorkloadType::Graphics;
        auto cmdBuffer = Flourish::CommandBuffer::Create(cmdCreateInfo);

        Flourish::BufferCreateInfo bufCreateInfo;
        bufCreateInfo.Type = Flourish::BufferType::Uniform;
        bufCreateInfo.Usage = Flourish::BufferUsageType::Dynamic;
        bufCreateInfo.Layout = { { Flourish::BufferDataType::Float4 } };
        bufCreateInfo.ElementCount = 1;
        auto buffer = Flourish::Buffer::Create(bufCreateInfo);

        float val = 3.f;
        buffer->SetBytes(&val, sizeof(float), 0);
        buffer->Flush();
    }

    Flourish::Context::Shutdown();

    return 0;
}