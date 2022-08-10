#pragma once

namespace Flourish
{
    enum class LogLevel
    {
        Trace = 0,
        Debug = 1,
        Info = 2,
        Warn = 3,
        Error = 4,
        Critical = 5
    };
    
    using LogFn = std::function<void(LogLevel, const char*)>;

    class Logger
    {
    public:
        inline static void SetLogFunction(LogFn&& func) { s_Log = func; }

        template <class... Args>
        inline static void Log(LogLevel level, const char* message, Args... args)
        {
            #ifdef FL_LOGGING
                if (!s_Log) return;
                char buffer[1000];
                sprintf(buffer, message, std::forward<Args>(args)...);
                s_Log(level, buffer);
            #endif
        }

    private:
        inline static LogFn s_Log;
    };
}

#ifdef FL_LOGGING
    #define FL_LOG_TRACE(...) ::Flourish::Logger::Log(::Flourish::LogLevel::Trace, __VA_ARGS__)
    #define FL_LOG_DEBUG(...) ::Flourish::Logger::Log(::Flourish::LogLevel::Debug, __VA_ARGS__)
    #define FL_LOG_INFO(...) ::Flourish::Logger::Log(::Flourish::LogLevel::Info, __VA_ARGS__)
    #define FL_LOG_WARN(...) ::Flourish::Logger::Log(::Flourish::LogLevel::Warn, __VA_ARGS__)
    #define FL_LOG_ERROR(...) ::Flourish::Logger::Log(::Flourish::LogLevel::Error, __VA_ARGS__)
    #define FL_LOG_CRITICAL(...) ::Flourish::Logger::Log(::Flourish::LogLevel::Critical, __VA_ARGS__)
#else
    #define FL_LOG_TRACE(...)
    #define FL_LOG_DEBUG(...)
    #define FL_LOG_INFO(...)
    #define FL_LOG_WARN(...)
    #define FL_LOG_ERROR(...)
    #define FL_LOG_CRITICAL(...)
#endif