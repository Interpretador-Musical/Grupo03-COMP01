cmake_minimum_required(VERSION 3.10)

project(InterpretadorMusical LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(FLEX REQUIRED)
find_package(BISON REQUIRED)

set(LEXER_FILE ${CMAKE_CURRENT_SOURCE_DIR}/lexer/lexer.l)
set(PARSER_FILE ${CMAKE_CURRENT_SOURCE_DIR}/parser/parser.y)

BISON_TARGET(Parser ${PARSER_FILE} ${CMAKE_CURRENT_BINARY_DIR}/parser.tab.c COMPILE_FLAGS "-d")

FLEX_TARGET(Lexer ${LEXER_FILE} ${CMAKE_CURRENT_BINARY_DIR}/lex.yy.c)

ADD_FLEX_BISON_DEPENDENCY(Lexer Parser)

add_executable(compilador
    ${BISON_Parser_OUTPUTS}
    ${FLEX_Lexer_OUTPUTS}
    ${CMAKE_CURRENT_SOURCE_DIR}/audio.cpp
)

target_include_directories(compilador PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

target_link_libraries(compilador PRIVATE pthread m dl)