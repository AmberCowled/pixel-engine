#pragma once

#include <spdlog/sinks/base_sink.h>
#include <mutex>
#include <vector>
#include <string>

namespace PixelEngine {

    struct LogMessage {
        std::string Message;
        spdlog::level::level_enum Level;
    };

    class EditorConsoleSink : public spdlog::sinks::base_sink<std::mutex> {
    public:
        static const std::vector<LogMessage>& GetMessages() {
            std::lock_guard<std::mutex> lock(s_BufferMutex);
            return s_MessageBuffer;
        }

        static void Clear() {
            std::lock_guard<std::mutex> lock(s_BufferMutex);
            s_MessageBuffer.clear();
        }

    protected:
        void sink_it_(const spdlog::details::log_msg& msg) override {
            spdlog::memory_buf_t formatted;
            spdlog::sinks::base_sink<std::mutex>::formatter_->format(msg, formatted);
            
            std::lock_guard<std::mutex> lock(s_BufferMutex);
            s_MessageBuffer.push_back({ fmt::to_string(formatted), msg.level });
        }

        void flush_() override {}

    private:
        static inline std::mutex s_BufferMutex;
        static inline std::vector<LogMessage> s_MessageBuffer;
    };

}
