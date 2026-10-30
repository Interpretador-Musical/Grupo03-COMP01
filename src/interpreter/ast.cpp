#include "interpreter/ast.h"

namespace mus {
namespace ast {

void liberar(Expressao* expressao) {
    if (expressao == nullptr) {
        return;
    }
    if (auto* unaria = std::get_if<ExpressaoUnaria>(expressao)) {
        liberar(unaria->operando);
    } else if (auto* binaria = std::get_if<ExpressaoBinaria>(expressao)) {
        liberar(binaria->esquerda);
        liberar(binaria->direita);
    }
    delete expressao;
}

void liberar(Comando* comando) {
    if (comando == nullptr) {
        return;
    }
    if (auto* atribuicao = std::get_if<ComandoAtribuicao>(comando)) {
        liberar(atribuicao->valor);
    } else if (auto* toca = std::get_if<ComandoToca>(comando)) {
        liberar(toca->altura);
        liberar(toca->duracao);
    } else if (auto* pausa = std::get_if<ComandoPausa>(comando)) {
        liberar(pausa->duracao);
    } else if (auto* andamento = std::get_if<ComandoAndamento>(comando)) {
        liberar(andamento->valor);
    } else if (auto* oitava = std::get_if<ComandoOitava>(comando)) {
        liberar(oitava->valor);
    } else if (auto* volume = std::get_if<ComandoVolume>(comando)) {
        liberar(volume->valor);
    } else if (auto* repita = std::get_if<ComandoRepita>(comando)) {
        liberar(repita->vezes);
        liberar(repita->corpo);
    }
    delete comando;
}

void liberar(std::vector<Comando*>* comandos) {
    if (comandos == nullptr) {
        return;
    }
    for (Comando* comando : *comandos) {
        liberar(comando);
    }
    delete comandos;
}

}  // namespace ast
}  // namespace mus
