#include "cli/logger.h"

#include <atomic>
#include <cstdio>
#include <mutex>

#ifdef _WIN32
#include <io.h>
#define MUS_ISATTY() (_isatty(2) != 0)
#else
#include <unistd.h>
#define MUS_ISATTY() (isatty(STDERR_FILENO) != 0)
#endif

namespace mus {
namespace {

// Atômico porque o nível é lido pela thread de tempo enquanto a principal
// ainda pode estar processando as opções de linha de comando.
std::atomic<LogLevel> g_level{LogLevel::Info};
std::mutex g_writeMutex;

// Cor só quando stderr é um terminal de verdade: em pipe ou arquivo de log os
// códigos ANSI virariam lixo.
bool colorsEnabled() {
    static const bool enabled = MUS_ISATTY();
    return enabled;
}

struct LevelStyle {
    const char* label;
    const char* color;
};

LevelStyle styleFor(LogLevel level) {
    switch (level) {
        case LogLevel::Debug:
            return {"DEBUG", "\033[90m"};
        case LogLevel::Info:
            return {"INFO ", "\033[36m"};
        case LogLevel::Warn:
            return {"WARN ", "\033[33m"};
        case LogLevel::Error:
            return {"ERROR", "\033[31m"};
        default:
            return {"?????", ""};
    }
}

}  // namespace

void setLogLevel(LogLevel level) {
    g_level.store(level, std::memory_order_relaxed);
}

LogLevel logLevel() {
    return g_level.load(std::memory_order_relaxed);
}

void logLine(LogLevel level, const std::string& message) {
    if (level < logLevel()) {
        return;
    }

    const LevelStyle style = styleFor(level);

    std::string line;
    line.reserve(message.size() + 32);
    line += "[";
    if (colorsEnabled()) {
        line += style.color;
    }
    line += style.label;
    if (colorsEnabled()) {
        line += "\033[0m";
    }
    line += "] ";
    line += message;
    line += "\n";

    std::lock_guard<std::mutex> lock(g_writeMutex);
    std::fwrite(line.data(), 1, line.size(), stderr);
    std::fflush(stderr);
}

}  // namespace mus
