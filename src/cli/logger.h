/**
 * @file logger.h
 * @brief Logger thread-safe via `stderr` com níveis configuráveis.
 */

#ifndef MUS_CLI_LOGGER_H
#define MUS_CLI_LOGGER_H

#include <sstream>
#include <string>

namespace mus {

/**
 * @brief Níveis de severidade suportados pelo logger da CLI.
 */
enum class LogLevel {
    Debug = 0,  ///< Mensagens detalhadas ativadas por `-v` / `--verbose`.
    Info = 1,   ///< Informações operacionais padrão.
    Warn = 2,   ///< Avisos recuperáveis (ex.: ausência de placa de som).
    Error = 3,  ///< Erros exibidos mesmo com `-q` / `--quiet`.
    Silent = 4  ///< Suprime todas as mensagens de log.
};

/**
 * @brief Define o nível mínimo global de log exibido.
 * @param level Novo limiar mínimo (`LogLevel`).
 */
void setLogLevel(LogLevel level);

/**
 * @brief Consulta o nível mínimo de log configurado atualmente.
 * @return Nível `LogLevel` ativo.
 */
LogLevel logLevel();

/**
 * @brief Emite uma linha formatada em `stderr` em escrita atômica única.
 *
 * Evita que logs concorrentes da thread de tempo (SCH-01, #4) se entrelacem
 * com os da thread principal.
 *
 * @param level Severidade da mensagem.
 * @param message Conteúdo textual da linha.
 */
void logLine(LogLevel level, const std::string& message);

/**
 * @brief Fluxo RAII que acumula partes com `<<` e emite a linha no destrutor.
 */
class LogStream {
public:
    /**
     * @brief Inicia um buffer de mensagem com a severidade informada.
     * @param level Nível da mensagem a emitir.
     */
    explicit LogStream(LogLevel level) : level_(level) {}

    /** @brief Envia a linha acumulada para `logLine()`. */
    ~LogStream() { logLine(level_, buffer_.str()); }

    LogStream(const LogStream&) = delete;
    LogStream& operator=(const LogStream&) = delete;

    /**
     * @brief Anexa um valor ao buffer da mensagem de log.
     * @tparam T Tipo streamável em `std::ostringstream`.
     * @param value Valor a formatar.
     * @return Referência para `*this` permitindo encadeamento.
     */
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

/** @def LOG_DEBUG
 *  @brief Emite uma mensagem de depuração (`LogLevel::Debug`).
 */
#define LOG_DEBUG ::mus::LogStream(::mus::LogLevel::Debug)
/** @def LOG_INFO
 *  @brief Emite uma mensagem informativa (`LogLevel::Info`).
 */
#define LOG_INFO ::mus::LogStream(::mus::LogLevel::Info)
/** @def LOG_WARN
 *  @brief Emite um aviso (`LogLevel::Warn`).
 */
#define LOG_WARN ::mus::LogStream(::mus::LogLevel::Warn)
/** @def LOG_ERROR
 *  @brief Emite uma mensagem de erro (`LogLevel::Error`).
 */
#define LOG_ERROR ::mus::LogStream(::mus::LogLevel::Error)

#endif  // MUS_CLI_LOGGER_H
