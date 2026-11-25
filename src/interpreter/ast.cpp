/**
 * @file ast.cpp
 * @brief Implementação das rotinas de liberação recursiva dos nós da AST.
 */

#include "interpreter/ast.h"

namespace mus {
namespace ast {

/**
 * @brief Desaloca uma árvore de expressão de forma recursiva.
 * @param expressao Nó de expressão a destruir (ignorado se `nullptr`).
 */
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

/**
 * @brief Desaloca um comando e todas as expressões/blocos sob sua posse.
 * @param comando Nó de comando a destruir (ignorado se `nullptr`).
 */
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
    } else if (auto* grupo = std::get_if<ComandoGrupo>(comando)) {
        liberar(grupo->trilhas);
    }
    delete comando;
}

/**
 * @brief Desaloca uma lista de comandos e o próprio contêiner `std::vector`.
 * @param comandos Vetor de comandos a destruir (ignorado se `nullptr`).
 */
void liberar(std::vector<Comando*>* comandos) {
    if (comandos == nullptr) {
        return;
    }
    for (Comando* comando : *comandos) {
        liberar(comando);
    }
    delete comandos;
}

/**
 * @brief Desaloca todas as trilhas de um grupo paralelo e o vetor externo.
 * @param trilhas Vetor de trilhas a destruir (ignorado se `nullptr`).
 */
void liberar(std::vector<std::vector<Comando*>*>* trilhas) {
    if (trilhas == nullptr) {
        return;
    }
    for (std::vector<Comando*>* trilha : *trilhas) {
        liberar(trilha);
    }
    delete trilhas;
}

}  // namespace ast
}  // namespace mus
