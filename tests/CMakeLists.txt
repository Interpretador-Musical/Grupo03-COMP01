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
    test_arena.cpp
    test_ast.cpp
    test_memoria.cpp
    test_profundidade.cpp
    test_encadeamento.cpp
    test_interpreter.cpp
    test_ambiente.cpp
    test_tabela_simbolos.cpp
    test_analisador_semantico.cpp
    test_timeline_player.cpp
    test_timeline_emit.cpp
    test_calculos.cpp
    test_representacao_intermediaria.cpp
    test_integracao.cpp
    test_ring_buffer.cpp
    test_timeline_queue.cpp
    test_trilhas.cpp
    test_audio_buffer.cpp
    test_sample_bank.cpp
    test_audio_envelope.cpp
    test_limiter.cpp
    test_wavetable.cpp
    test_volume_master.cpp
    test_biquad_filter.cpp
    test_audio_engine.cpp
    test_audio_underrun.cpp
    test_estresse.cpp
    test_perfil.cpp
    test_concorrencia.cpp
    test_ocioso.cpp
    test_drift.cpp
    test_dobra_constantes.cpp
    test_acucar_sintatico.cpp
    test_gerador_wav.cpp
    ${CMAKE_SOURCE_DIR}/src/codegen/gerador_wav.cpp
    ${CMAKE_SOURCE_DIR}/src/audio/audio_engine.cpp
    ${CMAKE_SOURCE_DIR}/src/audio/audio_buffer.cpp
    ${CMAKE_SOURCE_DIR}/src/audio/sample_bank.cpp
)

# include/ como SYSTEM: doctest e miniaudio são vendorizados, e os avisos
# deles não são nossos (CMP-12, #64).
target_include_directories(mus_tests SYSTEM PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_include_directories(mus_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)

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
    ${CMAKE_SOURCE_DIR}/src/audio/sample_bank.cpp
)

target_include_directories(mus_estresse SYSTEM PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_include_directories(mus_estresse PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(mus_estresse PRIVATE mus_lexer)

# Medição dos chiados (BUG-AUDIO, #109): imprime a tabela de antes e depois de
# `docs/chiados.md`. Fica fora do CTest, como o mus_estresse. A regressão da
# #109 está em `test_limiter.cpp`.
add_executable(mus_chiados
    chiados/medir_chiados.cpp
    ${CMAKE_SOURCE_DIR}/src/audio/audio_engine.cpp
    ${CMAKE_SOURCE_DIR}/src/audio/audio_buffer.cpp
    ${CMAKE_SOURCE_DIR}/src/audio/sample_bank.cpp
)

target_include_directories(mus_chiados SYSTEM PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_include_directories(mus_chiados PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(mus_chiados PRIVATE mus_lexer mus_avisos)
target_compile_definitions(mus_chiados PRIVATE MUS_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

# Dependências de plataforma da miniaudio. 
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(mus_tests PRIVATE m dl)
    target_link_libraries(mus_estresse PRIVATE m dl)
    target_link_libraries(mus_chiados PRIVATE m dl)
elseif(APPLE)
    foreach(alvo mus_tests mus_estresse mus_chiados)
        target_link_libraries(${alvo} PRIVATE
            "-framework CoreFoundation"
            "-framework CoreAudio"
            "-framework AudioToolbox")
    endforeach()
endif()

# Os testes seguem os mesmos avisos do resto do projeto (CMP-12, #64; ver o
# CMakeLists.txt da raiz).
target_link_libraries(mus_tests PRIVATE mus_avisos)
target_link_libraries(mus_estresse PRIVATE mus_avisos)

# Um teste do ctest por subcaso, para que a saída do CI aponte exatamente o
# que quebrou em vez de um único "mus_tests falhou".
add_test(NAME mus_tests COMMAND mus_tests --reporters=console --no-intro
    --test-suite-exclude=estresse,concorrencia,ocioso)

set_tests_properties(mus_tests PROPERTIES TIMEOUT 60)

# Teste de estresse do pipeline batch (INT-07, #43). Fica numa entrada própria
# do ctest para aparecer separado na saída e ter prazo próprio: ele renderiza
# cerca de um minuto de áudio fora do dispositivo. Os jobs de sanitizer chamam
# o `mus_tests` direto, então rodam esta suíte junto com as outras.
add_test(NAME mus_tests_estresse COMMAND mus_tests --reporters=console --no-intro
    --test-suite=estresse)

set_tests_properties(mus_tests_estresse PROPERTIES TIMEOUT 300)

# Concorrência (SCH-08, #46): casos com threads de verdade, inclusive a de
# áudio do miniaudio pelo backend null. Entrada própria porque tocam em tempo
# real; o job ThreadSanitizer do CI roda o `mus_tests` direto e os inclui.
add_test(NAME mus_tests_concorrencia COMMAND mus_tests --reporters=console --no-intro
    --test-suite=concorrencia)

set_tests_properties(mus_tests_concorrencia PROPERTIES TIMEOUT 60)

# Consumo ocioso (SCH-12, #66): mede o tempo de CPU do processo enquanto as
# threads esperam. Entrada própria porque sobe threads reais e mede tempo; o
# limite tem folga para os sanitizers, que rodam o `mus_tests` direto.
add_test(NAME mus_tests_ocioso COMMAND mus_tests --reporters=console --no-intro
    --test-suite=ocioso)

set_tests_properties(mus_tests_ocioso PROPERTIES TIMEOUT 60)

# Testes de interface do `--emit-timeline` (COMP-B, #105): rodam o executável
# `compilador` de verdade e comparam o stdout com a saída esperada, guardada em
# `emit_timeline/`. Se uma mudança no interpretador alterar a timeline de
# propósito, regenere o .csv com
#   ./build/compilador -q --emit-timeline exemplos/<nome>.mus > tests/emit_timeline/<nome>.csv
# e confira o diff antes de commitar.
set(EMIT_TIMELINE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/emit_timeline)

foreach(exemplo escala arpejo polirritmia)
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

foreach(caso notas_e_pausas andamento repeticao expressoes trilhas)
    add_test(NAME ir_${caso}
        COMMAND ${CMAKE_COMMAND}
            -DCOMPILADOR=$<TARGET_FILE:compilador>
            -DENTRADA=${IR_DIR}/${caso}.mus
            -DESPERADO=${IR_DIR}/${caso}.csv
            -P ${EMIT_TIMELINE_DIR}/verifica.cmake)
endforeach()

# Os casos `ir_recusa_*` são programas que passam pela semântica e que o
# interpretador recusa pelo valor calculado (#104, #124).
foreach(caso volume_invalido repita_infinito oitava_invalida)
    add_test(NAME ir_recusa_${caso}
        COMMAND ${CMAKE_COMMAND}
            -DCOMPILADOR=$<TARGET_FILE:compilador>
            -DENTRADA=${IR_DIR}/${caso}.mus
            -P ${EMIT_TIMELINE_DIR}/verifica.cmake)
endforeach()

# Suíte de integração ponta a ponta (INT-06, #37):
# Valida o executável compilador em cenários de sucesso e falhas seguras via CTest.
add_test(NAME integracao_cli_escala_no_audio
    COMMAND $<TARGET_FILE:compilador> -q --no-audio ${CMAKE_SOURCE_DIR}/exemplos/escala.mus)

add_test(NAME integracao_cli_arpejo_no_audio
    COMMAND $<TARGET_FILE:compilador> -q --no-audio ${CMAKE_SOURCE_DIR}/exemplos/arpejo.mus)

add_test(NAME integracao_cli_polirritmia_no_audio
    COMMAND $<TARGET_FILE:compilador> -q --no-audio ${CMAKE_SOURCE_DIR}/exemplos/polirritmia.mus)

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

add_test(NAME integracao_cli_volume_master_valido
    COMMAND $<TARGET_FILE:compilador> -q --no-audio --volume 0.5 ${CMAKE_SOURCE_DIR}/exemplos/escala.mus)

add_test(NAME integracao_cli_recusa_volume_master_invalido
    COMMAND $<TARGET_FILE:compilador> -q --no-audio --volume 1.5 ${CMAKE_SOURCE_DIR}/exemplos/escala.mus)
set_tests_properties(integracao_cli_recusa_volume_master_invalido PROPERTIES WILL_FAIL TRUE)

add_test(NAME integracao_cli_lpf_valido
    COMMAND $<TARGET_FILE:compilador> -q --no-audio --lpf 800 ${CMAKE_SOURCE_DIR}/exemplos/escala.mus)

add_test(NAME integracao_cli_recusa_lpf_invalido
    COMMAND $<TARGET_FILE:compilador> -q --no-audio --lpf 10 ${CMAKE_SOURCE_DIR}/exemplos/escala.mus)
set_tests_properties(integracao_cli_recusa_lpf_invalido PROPERTIES WILL_FAIL TRUE)

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

# Acima do teto de eventos (#125): precisa ser recusado, com código diferente
# de 0 e nada no stdout, em vez de esgotar a memória.
add_test(NAME estresse_cli_recusa_acima_do_teto
    COMMAND ${CMAKE_COMMAND}
        -DCOMPILADOR=$<TARGET_FILE:compilador>
        -DENTRADA=${ESTRESSE_DIR}/acima_do_teto.mus
        -P ${EMIT_TIMELINE_DIR}/verifica.cmake)

set_tests_properties(estresse_cli_denso_1500bpm estresse_cli_mista
    estresse_cli_um_milhao_no_audio estresse_cli_recusa_acima_do_teto
    PROPERTIES TIMEOUT 60)

# Testes de interface da saída-alvo WAV (`-o` / `--saida`, COMP-C, #108):
# validam o executável de verdade, cabeçalho RIFF/WAVE, tamanho exato em
# bytes, determinismo entre `-o`, `--saida` e `--sem-otimizacao`, e recusa
# segura de entrada ou caminho de saída inválido.
set(SAIDA_WAV_DIR ${CMAKE_CURRENT_SOURCE_DIR}/saida_wav)

add_test(NAME saida_wav_escala
    COMMAND ${CMAKE_COMMAND}
        -DCOMPILADOR=$<TARGET_FILE:compilador>
        -DENTRADA=${CMAKE_SOURCE_DIR}/exemplos/escala.mus
        -DMODO=NORMAL
        -DESPERADO_FRAMES=238140
        -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/saida_wav_tmp/escala
        -P ${SAIDA_WAV_DIR}/verifica.cmake)

add_test(NAME saida_wav_arpejo
    COMMAND ${CMAKE_COMMAND}
        -DCOMPILADOR=$<TARGET_FILE:compilador>
        -DENTRADA=${CMAKE_SOURCE_DIR}/exemplos/arpejo.mus
        -DMODO=NORMAL
        -DESPERADO_FRAMES=176400
        -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/saida_wav_tmp/arpejo
        -P ${SAIDA_WAV_DIR}/verifica.cmake)

add_test(NAME saida_wav_polirritmia
    COMMAND ${CMAKE_COMMAND}
        -DCOMPILADOR=$<TARGET_FILE:compilador>
        -DENTRADA=${CMAKE_SOURCE_DIR}/exemplos/polirritmia.mus
        -DMODO=NORMAL
        -DESPERADO_FRAMES=264600
        -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/saida_wav_tmp/polirritmia
        -P ${SAIDA_WAV_DIR}/verifica.cmake)

add_test(NAME saida_wav_recusa_erro_sintaxe
    COMMAND ${CMAKE_COMMAND}
        -DCOMPILADOR=$<TARGET_FILE:compilador>
        -DENTRADA=${EMIT_TIMELINE_DIR}/erro_sintaxe.mus
        -DMODO=RECUSA_ENTRADA
        -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/saida_wav_tmp/recusa_sintaxe
        -P ${SAIDA_WAV_DIR}/verifica.cmake)

add_test(NAME saida_wav_recusa_erro_semantico
    COMMAND ${CMAKE_COMMAND}
        -DCOMPILADOR=$<TARGET_FILE:compilador>
        -DENTRADA=${CMAKE_SOURCE_DIR}/exemplos/erros_semanticos.mus
        -DMODO=RECUSA_ENTRADA
        -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/saida_wav_tmp/recusa_semantica
        -P ${SAIDA_WAV_DIR}/verifica.cmake)

add_test(NAME saida_wav_recusa_caminho_invalido
    COMMAND ${CMAKE_COMMAND}
        -DCOMPILADOR=$<TARGET_FILE:compilador>
        -DENTRADA=${CMAKE_SOURCE_DIR}/exemplos/escala.mus
        -DMODO=RECUSA_SAIDA
        -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/saida_wav_tmp/recusa_saida
        -P ${SAIDA_WAV_DIR}/verifica.cmake)

set_tests_properties(
    saida_wav_escala
    saida_wav_arpejo
    saida_wav_polirritmia
    saida_wav_recusa_erro_sintaxe
    saida_wav_recusa_erro_semantico
    saida_wav_recusa_caminho_invalido
    PROPERTIES TIMEOUT 180)

# SCH-11 (#61): auditoria das restrições de tempo real do callback.
# Python 3 é opcional na configuração e usado apenas nos testes, nunca no áudio.
find_package(Python3 COMPONENTS Interpreter)

if(Python3_FOUND)
    add_test(
        NAME sch11_auditoria_realtime
        COMMAND ${Python3_EXECUTABLE} -B
            ${CMAKE_SOURCE_DIR}/scripts/check_realtime.py
            --root ${CMAKE_SOURCE_DIR}
    )

    add_test(
        NAME sch11_testes_verificador
        COMMAND ${Python3_EXECUTABLE} -B -m unittest discover
            -s ${CMAKE_SOURCE_DIR}/scripts
            -p test_check_realtime.py
            -v
    )

    set_tests_properties(
        sch11_auditoria_realtime
        sch11_testes_verificador
        PROPERTIES TIMEOUT 30
    )
endif()
