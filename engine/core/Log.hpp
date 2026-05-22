#pragma once

#include <memory>
#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>

namespace PixelEngine {

    class Log {
    public:
        static void Init();

        static std::shared_ptr<spdlog::logger>& GetCoreLogger() { return s_CoreLogger; }
        static std::shared_ptr<spdlog::logger>& GetClientLogger() { return s_ClientLogger; }

    private:
        static std::shared_ptr<spdlog::logger> s_CoreLogger;
        static std::shared_ptr<spdlog::logger> s_ClientLogger;
    };

}

// Core log macros
#define PX_CORE_TRACE(...)    ::PixelEngine::Log::GetCoreLogger()->trace(__VA_ARGS__)
#define PX_CORE_INFO(...)     ::PixelEngine::Log::GetCoreLogger()->info(__VA_ARGS__)
#define PX_CORE_WARN(...)     ::PixelEngine::Log::GetCoreLogger()->warn(__VA_ARGS__)
#define PX_CORE_ERROR(...)    ::PixelEngine::Log::GetCoreLogger()->error(__VA_ARGS__)
#define PX_CORE_CRITICAL(...) ::PixelEngine::Log::GetCoreLogger()->critical(__VA_ARGS__)

// Client log macros
#define PX_TRACE(...)         ::PixelEngine::Log::GetClientLogger()->trace(__VA_ARGS__)
#define PX_INFO(...)          ::PixelEngine::Log::GetClientLogger()->info(__VA_ARGS__)
#define PX_WARN(...)          ::PixelEngine::Log::GetClientLogger()->warn(__VA_ARGS__)
#define PX_ERROR(...)         ::PixelEngine::Log::GetClientLogger()->error(__VA_ARGS__)
#define PX_CRITICAL(...)      ::PixelEngine::Log::GetClientLogger()->critical(__VA_ARGS__)
