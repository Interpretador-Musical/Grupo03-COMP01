# Teste de interface do `--emit-timeline` (COMP-B, #105). Roda o executável de
# verdade, do jeito que um usuário roda, o que a suíte doctest não alcança: ela
# linka as bibliotecas, mas não o `main.cpp`.
#
#   cmake -DCOMPILADOR=<exe> -DENTRADA=<.mus> [-DESPERADO=<.csv>] -P verifica.cmake
#
# Com ESPERADO: o programa precisa compilar (código 0) e o stdout precisa ser
# idêntico ao arquivo, byte a byte. Roda duas vezes para checar determinismo.
#
# Sem ESPERADO: o programa é inválido de propósito. Precisa sair com código
# diferente de 0 e sem nada no stdout.

function(emitir saida_var codigo_var)
    execute_process(
        COMMAND "${COMPILADOR}" --quiet --emit-timeline "${ENTRADA}"
        OUTPUT_VARIABLE saida
        ERROR_VARIABLE erros
        RESULT_VARIABLE codigo)
    set(${saida_var} "${saida}" PARENT_SCOPE)
    set(${codigo_var} "${codigo}" PARENT_SCOPE)
    set(ultimos_erros "${erros}" PARENT_SCOPE)
endfunction()

emitir(primeira codigo)

if(NOT DEFINED ESPERADO)
    if(codigo EQUAL 0)
        message(FATAL_ERROR "'${ENTRADA}' deveria ser recusado, mas compilou")
    endif()
    if(NOT primeira STREQUAL "")
        message(FATAL_ERROR "programa inválido escreveu no stdout:\n${primeira}")
    endif()
    return()
endif()

if(NOT codigo EQUAL 0)
    message(FATAL_ERROR "'${ENTRADA}' falhou (código ${codigo}):\n${ultimos_erros}")
endif()

file(READ "${ESPERADO}" esperado)
if(NOT primeira STREQUAL esperado)
    message(FATAL_ERROR
        "saída diferente de ${ESPERADO}\n"
        "--- esperado\n${esperado}\n--- obtido\n${primeira}")
endif()

emitir(segunda codigo)
if(NOT segunda STREQUAL primeira)
    message(FATAL_ERROR "duas execuções seguidas deram saídas diferentes")
endif()
