#include "doctest.h"

#include "cli/logger.h"

TEST_CASE("os níveis de log são ordenados do mais falante ao mais silencioso") {
    // logLine() filtra com `level < logLevel()`, então essa ordem não é
    // decorativa: é ela que decide o que aparece.
    CHECK(mus::LogLevel::Debug < mus::LogLevel::Info);
    CHECK(mus::LogLevel::Info < mus::LogLevel::Warn);
    CHECK(mus::LogLevel::Warn < mus::LogLevel::Error);
    CHECK(mus::LogLevel::Error < mus::LogLevel::Silent);
}

TEST_CASE("setLogLevel é lido de volta por logLevel") {
    const mus::LogLevel original = mus::logLevel();

    mus::setLogLevel(mus::LogLevel::Debug);
    CHECK(mus::logLevel() == mus::LogLevel::Debug);

    mus::setLogLevel(mus::LogLevel::Silent);
    CHECK(mus::logLevel() == mus::LogLevel::Silent);

    mus::setLogLevel(original);
    CHECK(mus::logLevel() == original);
}

TEST_CASE("logar abaixo do nível corrente não escreve nada nem quebra") {
    const mus::LogLevel original = mus::logLevel();

    mus::setLogLevel(mus::LogLevel::Silent);
    // Silent é maior que qualquer nível emitido, então nada sai em stderr.
    // O que se verifica aqui é que o caminho de descarte é seguro — inclusive
    // o do LogStream, que só emite no destrutor.
    mus::logLine(mus::LogLevel::Error, "não deve aparecer");
    LOG_ERROR << "também não deve aparecer";

    mus::setLogLevel(original);
    CHECK(mus::logLevel() == original);
}
