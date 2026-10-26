#include "semantic/tabela_simbolos.h"

namespace mus {
namespace semantica {

const char* nomeTipo(Tipo tipo) {
    switch (tipo) {
        case Tipo::Numero:
            return "numero";
        case Tipo::Nota:
            return "nota";
        case Tipo::Booleano:
            return "booleano";
        case Tipo::Erro:
            return "erro";
    }
    return "?";  // inatingível: switch cobre todo Tipo
}

TabelaSimbolos::TabelaSimbolos() : escopos_(1) {}

void TabelaSimbolos::abrirEscopo() {
    escopos_.emplace_back();
}

void TabelaSimbolos::fecharEscopo() {
    if (escopos_.size() > 1) {
        escopos_.pop_back();
    }
}

std::size_t TabelaSimbolos::declarar(const std::string& nome, Tipo tipo,
                                     ast::Posicao posicao) {
    auto& escopo = escopos_.back();
    if (escopo.count(nome) != 0) {
        return kNaoEncontrado;
    }

    Simbolo simbolo;
    simbolo.nome = nome;
    simbolo.tipo = tipo;
    simbolo.nivelEscopo = nivelAtual();
    simbolo.endereco = simbolos_.size();
    simbolo.declaracao = posicao;

    const std::size_t indice = simbolos_.size();
    simbolos_.push_back(simbolo);
    escopo[nome] = indice;
    return indice;
}

std::size_t TabelaSimbolos::buscar(const std::string& nome) const {
    for (auto escopo = escopos_.rbegin(); escopo != escopos_.rend(); ++escopo) {
        const auto it = escopo->find(nome);
        if (it != escopo->end()) {
            return it->second;
        }
    }
    return kNaoEncontrado;
}

void TabelaSimbolos::registrarUso(std::size_t indice, ast::Posicao posicao) {
    simbolos_[indice].usos.push_back(posicao);
}

}  // namespace semantica
}  // namespace mus
