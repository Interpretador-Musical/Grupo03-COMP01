cmake_minimum_required(VERSION 3.16)

project(InterpretadorMusical VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(FLEX REQUIRED)
find_package(BISON REQUIRED)
find_package(Threads REQUIRED)

option(MUS_BUILD_TESTS "Compila a suíte de testes (ctest)" ON)

set(LEXER_FILE ${CMAKE_CURRENT_SOURCE_DIR}/lexer/lexer.l)
set(PARSER_FILE ${CMAKE_CURRENT_SOURCE_DIR}/parser/parser.y)

# Gerados como .cpp para que o projeto inteiro compile como C++17. Sem isso,
# ligar o parser ao restante do código exigiria envolver tudo em `extern "C"`
# assim que a AST passar a usar tipos de C++ (CMP-04, #21).
# DEFINES_FILE fixa o nome do cabeçalho: com saída .cpp o Bison geraria
# parser.tab.hpp, e o lexer.l inclui "parser.tab.h".
BISON_TARGET(Parser ${PARSER_FILE} ${CMAKE_CURRENT_BINARY_DIR}/parser.tab.cpp
    DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/parser.tab.h)

FLEX_TARGET(Lexer ${LEXER_FILE} ${CMAKE_CURRENT_BINARY_DIR}/lex.yy.cpp)

ADD_FLEX_BISON_DEPENDENCY(Lexer Parser)

# A lógica do interpretador vive numa biblioteca, e não direto no executável,
# para que os testes possam linká-la sem arrastar o main.cpp junto — dois
# `main` no mesmo binário não linkam. É o que viabiliza o MAT-03 (#16).
#
# Flex, Bison e a miniaudio ficam de fora de propósito: nada aqui depende
# deles, e mantê-los fora deixa o binário de teste rápido de recompilar.
add_library(mus_core STATIC
    src/core/core.cpp
    src/core/playhead.cpp
    src/engine/engine.cpp
    src/cli/logger.cpp
    src/cli/options.cpp
)

target_include_directories(mus_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(mus_core PUBLIC Threads::Threads)

# O frontend (lexer + parser + o invólucro C++ que os torna chamáveis) vive numa
# biblioteca própria, e não dentro do executável, porque senão não há como
# testá-lo: o `mus_tests` não pode linkar o `main.cpp` junto. É o mesmo motivo
# que criou o `mus_core`.
#
# E fica separado do `mus_core` de propósito. O `mus_core` continua sem depender
# de Flex e Bison, então mexer numa regra do lexer não recompila a matemática do
# playhead, e quem só quer os tipos do core não paga a geração de código.
add_library(mus_lexer STATIC
    src/frontend/frontend.cpp
    ${BISON_Parser_OUTPUTS}
    ${FLEX_Lexer_OUTPUTS}
)

# O diretório de build entra como PRIVATE porque só o próprio lexer inclui o
# `parser.tab.h`: os códigos de token do Bison não vazam para quem usa a
# biblioteca, que enxerga apenas o `TipoToken` de `frontend/frontend.h`.
target_include_directories(mus_lexer
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src
    PRIVATE ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(mus_lexer PUBLIC mus_core)

add_executable(compilador
    src/main.cpp
    src/audio/audio_engine.cpp
)

target_include_directories(compilador PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# O mus_lexer já linka o mus_core como PUBLIC; repetir aqui só gera aviso de
# biblioteca duplicada no linker.
target_link_libraries(compilador PRIVATE mus_lexer)

# Dependências de plataforma da miniaudio. A versão anterior linkava
# `pthread m dl` incondicionalmente, o que quebra no Windows — a plataforma
# alvo declarada na ata de 19/08.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(compilador PRIVATE m dl)
elseif(APPLE)
    target_link_libraries(compilador PRIVATE
        "-framework CoreFoundation"
        "-framework CoreAudio"
        "-framework AudioToolbox")
endif()

if(MUS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
