# Teste de CLI do estresse (INT-07, #43). Roda o `compilador` de verdade com
# `--emit-timeline` e confere quantos eventos saíram, sem guardar o CSV
# esperado inteiro: com milhares de linhas, o diff deixaria de ser legível, e a
# contagem é o que a carga promete.
#
#   cmake -DCOMPILADOR=<exe> -DENTRADA=<.mus> -DEVENTOS=<n> -P conta_eventos.cmake

execute_process(
    COMMAND "${COMPILADOR}" --quiet --emit-timeline "${ENTRADA}"
    OUTPUT_VARIABLE saida
    ERROR_VARIABLE erros
    RESULT_VARIABLE codigo)

if(NOT codigo EQUAL 0)
    message(FATAL_ERROR "'${ENTRADA}' falhou (código ${codigo}):\n${erros}")
endif()

# Uma linha por evento, mais o cabeçalho.
string(REGEX MATCHALL "\n" quebras "${saida}")
list(LENGTH quebras linhas)
math(EXPR obtidos "${linhas} - 1")

if(NOT obtidos EQUAL EVENTOS)
    message(FATAL_ERROR "'${ENTRADA}': esperados ${EVENTOS} eventos, saíram ${obtidos}")
endif()
