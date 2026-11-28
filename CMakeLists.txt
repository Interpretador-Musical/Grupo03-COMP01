cmake_minimum_required(VERSION 3.16)

project(InterpretadorMusical VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(FLEX REQUIRED)
find_package(BISON REQUIRED)
find_package(Threads REQUIRED)

option(MUS_BUILD_TESTS "Compila a suíte de testes (ctest)" ON)
option(ENABLE_ASAN "Habilita AddressSanitizer e UndefinedBehaviorSanitizer" OFF)
option(ENABLE_TSAN "Habilita ThreadSanitizer" OFF)

# DSP-08 (#47): sem tipo de build explícito, `cmake -S . -B build` compila em
# -O0 (sem otimização). Em geradores single-config, usa Release (-O3) por padrão
# quando nenhum sanitizer estiver ativo.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES AND NOT ENABLE_ASAN AND NOT ENABLE_TSAN)
    set(CMAKE_BUILD_TYPE Release CACHE STRING
        "Tipo de build (Debug, Release, RelWithDebInfo, MinSizeRel)" FORCE)
endif()

# Otimização segura de ponto flutuante em builds otimizados: -fno-math-errno
# evita atualizar errno em chamadas da libm (ex.: tanh no limitador).
# -ffast-math fica de fora de propósito porque permite eliminar checagens de
# std::isnan/std::isfinite usadas no softClip, sampleLinear e interpretador.
if(NOT MSVC)
    add_compile_options(
        $<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>>:-fno-math-errno>
    )
endif()

if(ENABLE_ASAN)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -g -O1)
    add_link_options(-fsanitize=address,undefined)
    # Liga o envenenamento de memória da arena da AST (MAT-10, ver
    # src/interpreter/arena.h): o Clang não tem macro própria para o ASan.
    add_compile_definitions(MUS_COM_ASAN=1)
elseif(ENABLE_TSAN)
    add_compile_options(-fsanitize=thread -g -O1)
    add_link_options(-fsanitize=thread)
endif()

# Avisos do compilador (CMP-12, #64). Valem para todo alvo nosso: a biblioteca,
# o frontend, o executável e os testes. O porquê de cada flag está em
# docs/analise_estatica.md. Ficam de fora o código gerado pelo Flex e pelo
# Bison (abaixo) e os cabeçalhos vendorizados em include/, que entram como
# SYSTEM. MUS_WERROR transforma aviso em erro; fica desligado por padrão para
# um compilador mais novo não travar a build de quem só quer compilar, e o CI o
# liga.
option(MUS_WERROR "Trata avisos do compilador como erro (ligado no CI)" OFF)

add_library(mus_avisos INTERFACE)

if(MSVC)
    target_compile_options(mus_avisos INTERFACE /W4)
    if(MUS_WERROR)
        target_compile_options(mus_avisos INTERFACE /WX)
    endif()
else()
    target_compile_options(mus_avisos INTERFACE
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
        -Wnon-virtual-dtor
        -Woverloaded-virtual
        -Wold-style-cast
        -Wcast-align
        -Wnull-dereference
        -Wformat=2
        -Wimplicit-fallthrough
    )
    if(MUS_WERROR)
        target_compile_options(mus_avisos INTERFACE -Werror)
    endif()
endif()

set(LEXER_FILE ${CMAKE_CURRENT_SOURCE_DIR}/lexer/lexer.l)
set(PARSER_FILE ${CMAKE_CURRENT_SOURCE_DIR}/parser/parser.y)

# Gerados como .cpp para que o projeto inteiro compile como C++17. Sem isso,
# ligar o parser ao restante do código exigiria envolver tudo em `extern "C"`
# desde que a AST passou a usar tipos de C++ (`%union` de ponteiros para
# `mus::ast::*`, INT-03 #19).
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
    src/cli/sample_paths.cpp
    src/interpreter/arena.cpp
    src/interpreter/ambiente.cpp
    src/interpreter/interpreter.cpp
    src/interpreter/operacoes.cpp
    src/otimizacao/dobra_constantes.cpp
    src/core/timeline_player.cpp
    src/core/timeline_emit.cpp
    src/core/timeline_queue.cpp
    src/semantic/tabela_simbolos.cpp
    src/semantic/analisador_semantico.cpp
)

target_include_directories(mus_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(mus_core PUBLIC Threads::Threads)
target_link_libraries(mus_core PRIVATE mus_avisos)
target_compile_definitions(mus_core PUBLIC
    MUS_VERSION="${PROJECT_VERSION}"
)

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
target_link_libraries(mus_lexer PRIVATE mus_avisos)

# O código gerado não é nosso, e o Bison 2.3 do macOS e o 3.8 do Ubuntu geram
# código diferente: um aviso nele não diz nada sobre a gramática. O lexer.l e o
# parser.y continuam revisados; só a saída do gerador fica em silêncio.
if(NOT MSVC)
    set_source_files_properties(${FLEX_Lexer_OUTPUTS} ${BISON_Parser_OUTPUTS}
        PROPERTIES COMPILE_OPTIONS -w)
endif()

add_executable(compilador
    src/main.cpp
    src/codegen/gerador_wav.cpp
    src/audio/audio_engine.cpp
    src/audio/audio_buffer.cpp
    src/audio/sample_bank.cpp
)

# INT-05: caminho da raiz do projeto para localizar os samples WAV.
target_compile_definitions(compilador PRIVATE
    MUS_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

# SYSTEM porque é a miniaudio vendorizada: os avisos dela não são nossos.
target_include_directories(compilador SYSTEM PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# O mus_lexer já linka o mus_core como PUBLIC; repetir aqui só gera aviso de
# biblioteca duplicada no linker.
target_link_libraries(compilador PRIVATE mus_lexer mus_avisos)

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
