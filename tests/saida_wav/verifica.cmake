# Teste de interface da saída-alvo WAV (`-o` / `--saida`, COMP-C, #108).
#
#   cmake -DCOMPILADOR=<exe> -DENTRADA=<.mus> -DMODO=<NORMAL|RECUSA_ENTRADA|RECUSA_SAIDA>
#         [-DESPERADO_FRAMES=<N>] -DWORK_DIR=<dir> -P verifica.cmake
#
# NORMAL: gera o WAV com `-o` e `--sem-otimizacao --saida`, conferindo código 0,
# stdout vazio, assinatura RIFF/WAVE, tamanho exato em bytes e determinismo
# bit a bit (SHA-256) entre as duas execuções.
# RECUSA_ENTRADA: entrada inválida deve falhar sem criar arquivo WAV.
# RECUSA_SAIDA: caminho de saída em diretório inexistente deve falhar com erro.

if(NOT DEFINED WORK_DIR)
    message(FATAL_ERROR "WORK_DIR é obrigatório")
endif()

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")

function(executar_saida flag caminho_wav saida_var erros_var codigo_var)
    execute_process(
        COMMAND "${COMPILADOR}" -q ${flag} "${caminho_wav}" "${ENTRADA}"
        OUTPUT_VARIABLE out
        ERROR_VARIABLE err
        RESULT_VARIABLE rc)
    set(${saida_var} "${out}" PARENT_SCOPE)
    set(${erros_var} "${err}" PARENT_SCOPE)
    set(${codigo_var} "${rc}" PARENT_SCOPE)
endfunction()

if(MODO STREQUAL "RECUSA_ENTRADA")
    set(alvo "${WORK_DIR}/nao_deve_existir.wav")
    executar_saida("-o" "${alvo}" out err rc)
    if(rc EQUAL 0)
        file(REMOVE_RECURSE "${WORK_DIR}")
        message(FATAL_ERROR "'${ENTRADA}' deveria ser recusado, mas gerou saída WAV")
    endif()
    if(err STREQUAL "")
        file(REMOVE_RECURSE "${WORK_DIR}")
        message(FATAL_ERROR "recusa de '${ENTRADA}' não produziu mensagem de erro")
    endif()
    if(EXISTS "${alvo}")
        file(REMOVE_RECURSE "${WORK_DIR}")
        message(FATAL_ERROR "arquivo WAV parcial/indevido foi criado para entrada inválida")
    endif()
    file(REMOVE_RECURSE "${WORK_DIR}")
    return()
endif()

if(MODO STREQUAL "RECUSA_SAIDA")
    set(alvo "${WORK_DIR}/diretorio_inexistente/saida.wav")
    executar_saida("-o" "${alvo}" out err rc)
    if(rc EQUAL 0)
        file(REMOVE_RECURSE "${WORK_DIR}")
        message(FATAL_ERROR "caminho de saída inválido deveria falhar, mas retornou 0")
    endif()
    if(err STREQUAL "")
        file(REMOVE_RECURSE "${WORK_DIR}")
        message(FATAL_ERROR "caminho de saída inválido não produziu mensagem de erro")
    endif()
    file(REMOVE_RECURSE "${WORK_DIR}")
    return()
endif()

if(NOT MODO STREQUAL "NORMAL")
    file(REMOVE_RECURSE "${WORK_DIR}")
    message(FATAL_ERROR "MODO desconhecido: ${MODO}")
endif()

set(wav1 "${WORK_DIR}/tmp1.wav")
set(wav2 "${WORK_DIR}/tmp2.wav")

executar_saida("-o" "${wav1}" out1 err1 rc1)
if(NOT rc1 EQUAL 0)
    file(REMOVE_RECURSE "${WORK_DIR}")
    message(FATAL_ERROR "'-o' falhou para '${ENTRADA}' (código ${rc1}):\n${err1}")
endif()
if(NOT out1 STREQUAL "")
    file(REMOVE_RECURSE "${WORK_DIR}")
    message(FATAL_ERROR "'-o' escreveu no stdout:\n${out1}")
endif()

execute_process(
    COMMAND "${COMPILADOR}" -q --sem-otimizacao --saida "${wav2}" "${ENTRADA}"
    OUTPUT_VARIABLE out2
    ERROR_VARIABLE err2
    RESULT_VARIABLE rc2)
if(NOT rc2 EQUAL 0)
    file(REMOVE_RECURSE "${WORK_DIR}")
    message(FATAL_ERROR "'--sem-otimizacao --saida' falhou para '${ENTRADA}' (código ${rc2}):\n${err2}")
endif()
if(NOT out2 STREQUAL "")
    file(REMOVE_RECURSE "${WORK_DIR}")
    message(FATAL_ERROR "'--sem-otimizacao --saida' escreveu no stdout:\n${out2}")
endif()

foreach(arq "${wav1}" "${wav2}")
    if(NOT EXISTS "${arq}")
        file(REMOVE_RECURSE "${WORK_DIR}")
        message(FATAL_ERROR "arquivo WAV não foi criado: ${arq}")
    endif()
endforeach()

file(READ "${wav1}" cabecalho LIMIT 12 HEX)
string(LENGTH "${cabecalho}" cab_len)
if(NOT cab_len EQUAL 24)
    file(REMOVE_RECURSE "${WORK_DIR}")
    message(FATAL_ERROR "cabeçalho WAV muito curto em '${wav1}'")
endif()
string(SUBSTRING "${cabecalho}" 0 8 riff_hex)
string(SUBSTRING "${cabecalho}" 16 8 wave_hex)
if(NOT riff_hex STREQUAL "52494646" OR NOT wave_hex STREQUAL "57415645")
    file(REMOVE_RECURSE "${WORK_DIR}")
    message(FATAL_ERROR "assinatura RIFF/WAVE inválida em '${wav1}': ${cabecalho}")
endif()

file(SIZE "${wav1}" tamanho_real)
math(EXPR tamanho_esperado "44 + 2 * ${ESPERADO_FRAMES}")
if(NOT tamanho_real EQUAL tamanho_esperado)
    file(REMOVE_RECURSE "${WORK_DIR}")
    message(FATAL_ERROR "tamanho inesperado em '${wav1}': esperado ${tamanho_esperado}, obtido ${tamanho_real}")
endif()

file(SHA256 "${wav1}" hash1)
file(SHA256 "${wav2}" hash2)
if(NOT hash1 STREQUAL hash2)
    file(REMOVE_RECURSE "${WORK_DIR}")
    message(FATAL_ERROR "saída WAV não determinística: -o=${hash1}, --sem-otimizacao --saida=${hash2}")
endif()

file(REMOVE_RECURSE "${WORK_DIR}")
