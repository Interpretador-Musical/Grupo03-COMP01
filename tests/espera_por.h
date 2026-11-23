#ifndef MUS_TESTS_ESPERA_POR_H
#define MUS_TESTS_ESPERA_POR_H

#include <chrono>
#include <thread>

namespace mus::testes {

// Espera até que `condicao` seja verdadeira ou o prazo estoure.
//
// Os testes com thread não podem afirmar TEMPO — o sistema operacional não tem
// obrigação nenhuma de respeitar o intervalo pedido, e afirmar "N ticks em T
// segundos" transformaria o CI em fonte de falso negativo. O que se afirma
// são propriedades: que avança, que para, que não retrocede. O prazo só
// existe para um defeito virar falha em vez de travar a suíte.
template <typename F>
bool esperaPor(F condicao,
               std::chrono::milliseconds prazo = std::chrono::milliseconds(2000)) {
    const auto limite = std::chrono::steady_clock::now() + prazo;
    while (std::chrono::steady_clock::now() < limite) {
        if (condicao()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return condicao();
}

}  // namespace mus::testes

#endif  // MUS_TESTS_ESPERA_POR_H
