# Suíte de testes do MAT-03 (#16).
#
# Um único executável reunindo todos os arquivos de teste: o doctest registra
# os casos por si só, e um binário só evita pagar a compilação do cabeçalho
# uma vez por arquivo de teste.
add_executable(mus_tests
    test_main.cpp
    test_logger.cpp
    test_options.cpp
    test_pitch.cpp
    test_arc.cpp
    test_hap.cpp
    test_pattern.cpp
    test_subdivision.cpp
    test_sound_event.cpp
    test_playhead.cpp
    test_engine.cpp
    test_clock.cpp
    test_lexer.cpp
    test_parser.cpp
    test_gramatica.cpp
    test_ast.cpp
    test_memoria.cpp
    test_profundidade.cpp
    test_interpreter.cpp
    test_tabela_simbolos.cpp
    test_analisador_semantico.cpp
    test_timeline_player.cpp
    test_timeline_emit.cpp
    test_calculos.cpp
    test_representacao_intermediaria.cpp
    test_integracao.cpp
    test_ring_buffer.cpp
    test_timeline_queue.cpp
    test_audio_buffer.cpp
    test_audio_envelope.cpp
    test_limiter.cpp
    test_audio_engine.cpp
    test_audio_underrun.cpp
    test_estresse.cpp
    ${CMAKE_SOURCE_DIR}/src/audio/audio_engine.cpp
    ${CMAKE_SOURCE_DIR}/src/audio/audio_buffer.cpp
)

target_include_directories(mus_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(mus_tests PRIVATE mus_lexer)

# Raiz do repositório, para os testes que leem os `.mus` de `exemplos/` e
# `tests/ir/` sem depender do diretório em que o ctest foi chamado.
target_compile_definitions(mus_tests PRIVATE MUS_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

# Medição do estresse: imprime a tabela de tempos de `docs/estresse.md`. Não é
# teste do ctest, porque tempo de relógio depende da máquina. Ver o comentário
# no topo do arquivo.
add_executable(mus_estresse
    estresse/medir_estresse.cpp
    ${CMAKE_SOURCE_DIR}/src/audio/audio_engine.cpp
    ${CMAKE_SOURCE_DIR}/src/audio/audio_buffer.cpp
)

target_include_directories(mus_estresse PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(mus_estresse PRIVATE mus_lexer)

# Dependências de plataforma da miniaudio. 
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(mus_tests PRIVATE m dl)
    target_link_libraries(mus_estresse PRIVATE m dl)
elseif(APPLE)
    foreach(alvo mus_tests mus_estresse)
        target_link_libraries(${alvo} PRIVATE
            "-framework CoreFoundation"
            "-framework CoreAudio"
            "-framework AudioToolbox")
    endforeach()
endif()

# Os testes são o lugar onde um aviso do compilador precisa doer.
if(MSVC)
    target_compile_options(mus_tests PRIVATE /W4)
else()
    target_compile_options(mus_tests PRIVATE -Wall -Wextra)
endif()

# Um teste do ctest por subcaso, para que a saída do CI aponte exatamente o
# que quebrou em vez de um único "mus_tests falhou".
add_test(NAME mus_tests COMMAND mus_tests --reporters=console --no-intro
    --test-suite-exclude=estresse)

set_tests_properties(mus_tests PROPERTIES TIMEOUT 60)

# Teste de estresse do pipeline batch (INT-07, #43). Fica numa entrada própria
# do ctest para aparecer separado na saída e ter prazo próprio: ele renderiza
# cerca de um minuto de áudio fora do dispositivo. Os jobs de sanitizer chamam
# o `mus_tests` direto, então rodam esta suíte junto com as outras.
add_test(NAME mus_tests_estresse COMMAND mus_tests --reporters=console --no-intro
    --test-suite=estresse)

set_tests_properties(mus_tests_estresse PROPERTIES TIMEOUT 300)

# Testes de interface do `--emit-timeline` (COMP-B, #105): rodam o executável
# `compilador` de verdade e comparam o stdout com a saída esperada, guardada em
# `emit_timeline/`. Se uma mudança no interpretador alterar a timeline de
# propósito, regenere o .csv com
#   ./build/compilador -q --emit-timeline exemplos/<nome>.mus > tests/emit_timeline/<nome>.csv
# e confira o diff antes de commitar.
set(EMIT_TIMELINE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/emit_timeline)

foreach(exemplo escala arpejo)
    add_test(NAME emit_timeline_${exemplo}
        COMMAND ${CMAKE_COMMAND}
            -DCOMPILADOR=$<TARGET_FILE:compilador>
            -DENTRADA=${CMAKE_SOURCE_DIR}/exemplos/${exemplo}.mus
            -DESPERADO=${EMIT_TIMELINE_DIR}/${exemplo}.csv
            -P ${EMIT_TIMELINE_DIR}/verifica.cmake)
endforeach()

add_test(NAME emit_timeline_recusa_erro_sintaxe
    COMMAND ${CMAKE_COMMAND}
        -DCOMPILADOR=$<TARGET_FILE:compilador>
        -DENTRADA=${EMIT_TIMELINE_DIR}/erro_sintaxe.mus
        -P ${EMIT_TIMELINE_DIR}/verifica.cmake)

add_test(NAME emit_timeline_recusa_erro_semantico
    COMMAND ${CMAKE_COMMAND}
        -DCOMPILADOR=$<TARGET_FILE:compilador>
        -DENTRADA=${CMAKE_SOURCE_DIR}/exemplos/erros_semanticos.mus
        -P ${EMIT_TIMELINE_DIR}/verifica.cmake)

# Exemplos verificáveis da representação intermediária (COMP-A, #104): cada
# `tests/ir/<caso>.mus` tem ao lado o `<caso>.csv` com a timeline esperada,
# conferida à mão contra as regras de tradução de
# `docs/representacao_intermediaria.md`. Usa o mesmo `verifica.cmake` do
# `--emit-timeline`; para regenerar um .csv, mesmo comando de lá.
set(IR_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ir)

foreach(caso notas_e_pausas andamento repeticao expressoes)
    add_test(NAME ir_${caso}
        COMMAND ${CMAKE_COMMAND}
            -DCOMPILADOR=$<TARGET_FILE:compilador>
            -DENTRADA=${IR_DIR}/${caso}.mus
            -DESPERADO=${IR_DIR}/${caso}.csv
            -P ${EMIT_TIMELINE_DIR}/verifica.cmake)
endforeach()

add_test(NAME ir_recusa_volume_invalido
    COMMAND ${CMAKE_COMMAND}
        -DCOMPILADOR=$<TARGET_FILE:compilador>
        -DENTRADA=${IR_DIR}/volume_invalido.mus
        -P ${EMIT_TIMELINE_DIR}/verifica.cmake)

# Suíte de integração ponta a ponta (INT-06, #37):
# Valida o executável compilador em cenários de sucesso e falhas seguras via CTest.
add_test(NAME integracao_cli_escala_no_audio
    COMMAND $<TARGET_FILE:compilador> -q --no-audio ${CMAKE_SOURCE_DIR}/exemplos/escala.mus)

add_test(NAME integracao_cli_arpejo_no_audio
    COMMAND $<TARGET_FILE:compilador> -q --no-audio ${CMAKE_SOURCE_DIR}/exemplos/arpejo.mus)

add_test(NAME integracao_cli_recusa_arquivo_inexistente
    COMMAND $<TARGET_FILE:compilador> -q ${CMAKE_SOURCE_DIR}/exemplos/arquivo_inexistente_int06.mus)
set_tests_properties(integracao_cli_recusa_arquivo_inexistente PROPERTIES WILL_FAIL TRUE)

add_test(NAME integracao_cli_recusa_erro_sintaxe
    COMMAND $<TARGET_FILE:compilador> -q ${EMIT_TIMELINE_DIR}/erro_sintaxe.mus)
set_tests_properties(integracao_cli_recusa_erro_sintaxe PROPERTIES WILL_FAIL TRUE)

# O programa tem erro só semântico: se a análise semântica for pulada, o
# interpretador o executa sem erro e o compilador sai com 0.
add_test(NAME integracao_cli_recusa_erro_semantico
    COMMAND $<TARGET_FILE:compilador> -q --no-audio ${CMAKE_CURRENT_SOURCE_DIR}/integracao/so_semantico.mus)
set_tests_properties(integracao_cli_recusa_erro_semantico PROPERTIES WILL_FAIL TRUE)

# Estresse pela CLI (INT-07, #43): o executável de verdade com as cargas de
# `estresse/`. As duas primeiras conferem a contagem de eventos do
# `--emit-timeline` contra a conta fechada escrita no topo de cada `.mus`; a
# de um milhão de eventos só compila (`--no-audio`), porque o CSV teria 1 milhão
# de linhas. O TIMEOUT é o que transforma travamento em falha.
set(ESTRESSE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/estresse)

add_test(NAME estresse_cli_denso_1500bpm
    COMMAND ${CMAKE_COMMAND}
        -DCOMPILADOR=$<TARGET_FILE:compilador>
        -DENTRADA=${ESTRESSE_DIR}/denso_1500bpm.mus
        -DEVENTOS=6000
        -P ${ESTRESSE_DIR}/conta_eventos.cmake)

add_test(NAME estresse_cli_mista
    COMMAND ${CMAKE_COMMAND}
        -DCOMPILADOR=$<TARGET_FILE:compilador>
        -DENTRADA=${ESTRESSE_DIR}/mista.mus
        -DEVENTOS=5000
        -P ${ESTRESSE_DIR}/conta_eventos.cmake)

add_test(NAME estresse_cli_um_milhao_no_audio
    COMMAND $<TARGET_FILE:compilador> -q --no-audio ${ESTRESSE_DIR}/um_milhao.mus)

set_tests_properties(estresse_cli_denso_1500bpm estresse_cli_mista
    estresse_cli_um_milhao_no_audio PROPERTIES TIMEOUT 60)
