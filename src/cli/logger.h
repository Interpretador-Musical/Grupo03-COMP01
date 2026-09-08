#ifndef MUS_CLI_LOGGER_H
#define MUS_CLI_LOGGER_H

#include <sstream>
#include <string>

namespace mus {

enum class LogLevel {
    Debug = 0,
    Info = 1,
    Warn = 2,
    Error = 3,
    Silent = 4
};

// Nível mínimo global: mensagens abaixo dele são descartadas.
void setLogLevel(LogLevel level);
LogLevel logLevel();

// Emite uma linha já formatada em stderr, em uma única escrita, para que os
// logs da thread de tempo (SCH-01, #4) não se entrelacem com os da principal.
void logLine(LogLevel level, const std::string& message);

// Acumula a mensagem e só emite no destrutor. É o que permite usar `<<` sem
// perder a atomicidade da linha.
class LogStream {
public:
    explicit LogStream(LogLevel level) : level_(level) {}

    ~LogStream() { logLine(level_, buffer_.str()); }

    LogStream(const LogStream&) = delete;
    LogStream& operator=(const LogStream&) = delete;

    template <typename T>
    LogStream& operator<<(const T& value) {
        buffer_ << value;
        return *this;
    }

private:
    LogLevel level_;
    std::ostringstream buffer_;
};

}  // namespace mus

#define LOG_DEBUG ::mus::LogStream(::mus::LogLevel::Debug)
#define LOG_INFO ::mus::LogStream(::mus::LogLevel::Info)
#define LOG_WARN ::mus::LogStream(::mus::LogLevel::Warn)
#define LOG_ERROR ::mus::LogStream(::mus::LogLevel::Error)

#endif  // MUS_CLI_LOGGER_H
