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
    test_ast.cpp
    test_memoria.cpp
    test_profundidade.cpp
    test_interpreter.cpp
    test_tabela_simbolos.cpp
    test_analisador_semantico.cpp
    test_timeline_player.cpp
    test_timeline_emit.cpp
    test_ring_buffer.cpp
    test_audio_buffer.cpp
    test_audio_envelope.cpp
    test_limiter.cpp
    ${CMAKE_SOURCE_DIR}/src/audio/audio_engine.cpp
    ${CMAKE_SOURCE_DIR}/src/audio/audio_buffer.cpp
)

target_include_directories(mus_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(mus_tests PRIVATE mus_lexer)

# Dependências de plataforma da miniaudio. 
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(mus_tests PRIVATE m dl)
elseif(APPLE)
    target_link_libraries(mus_tests PRIVATE
        "-framework CoreFoundation"
        "-framework CoreAudio"
        "-framework AudioToolbox")
endif()

# Os testes são o lugar onde um aviso do compilador precisa doer.
if(MSVC)
    target_compile_options(mus_tests PRIVATE /W4)
else()
    target_compile_options(mus_tests PRIVATE -Wall -Wextra)
endif()

# Um teste do ctest por subcaso, para que a saída do CI aponte exatamente o
# que quebrou em vez de um único "mus_tests falhou".
add_test(NAME mus_tests COMMAND mus_tests --reporters=console --no-intro)

set_tests_properties(mus_tests PROPERTIES TIMEOUT 60)

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
