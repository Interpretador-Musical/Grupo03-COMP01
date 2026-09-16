# Suíte de testes do MAT-03 (#16).
#
# Um único executável reunindo todos os arquivos de teste: o doctest registra
# os casos por si só, e um binário só evita pagar a compilação do cabeçalho
# uma vez por arquivo de teste.
add_executable(mus_tests
    test_main.cpp
    test_logger.cpp
    test_options.cpp
)

target_include_directories(mus_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(mus_tests PRIVATE mus_core)

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
