#include "flpch.h"

#include "Flourish/Api/Context.h"
#include "Flourish/Core/Log.h"

#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"

#include "FlourishTesting/Tests.h"

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
    
    // Init context and run tests
    Flourish::ContextInitializeInfo contextInitInfo;
    contextInitInfo.Backend = Flourish::BackendType::Vulkan;
    contextInitInfo.ApplicationName = "FlourishTesting";
    Flourish::Context::Initialize(contextInitInfo);
    auto tests = std::make_shared<FlourishTesting::Tests>();
    tests->Run();
    tests.reset();
    Flourish::Context::Shutdown();

    return 0;
}