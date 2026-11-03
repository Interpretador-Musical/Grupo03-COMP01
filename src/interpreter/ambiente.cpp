#include "interpreter/ambiente.h"

#include <algorithm>
#include <utility>

namespace mus {

Ambiente::Ambiente() : escopos_(1) {}

void Ambiente::abrirEscopo() {
    escopos_.emplace_back();
}

void Ambiente::fecharEscopo() {
    if (escopos_.size() > 1) {
        escopos_.pop_back();
    }
}

void Ambiente::definirLocal(const std::string& nome, Valor valor) {
    escopos_.back()[nome] = valor;
}

void Ambiente::atribuir(const std::string& nome, Valor valor) {
    for (auto escopo = escopos_.rbegin(); escopo != escopos_.rend(); ++escopo) {
        const auto it = escopo->find(nome);
        if (it != escopo->end()) {
            it->second = valor;
            return;
        }
    }
    escopos_.back()[nome] = valor;
}

const Valor* Ambiente::buscar(const std::string& nome) const {
    for (auto escopo = escopos_.rbegin(); escopo != escopos_.rend(); ++escopo) {
        const auto it = escopo->find(nome);
        if (it != escopo->end()) {
            return &it->second;
        }
    }
    return nullptr;
}

Ambiente::MapaVariaveis Ambiente::capturarEstado() const {
    MapaVariaveis captura;
    // Percorre do escopo global até o mais interno para que definições mais
    // internas sobrescrevam homônimas externas (shadowing léxico).
    for (const auto& escopo : escopos_) {
        for (const auto& [nome, valor] : escopo) {
            captura[nome] = valor;
        }
    }
    return captura;
}

Closure Closure::capturar(const Ambiente& ambiente,
                          std::vector<std::string> parametros,
                          PonteiroFuncao funcao) {
    Closure c;
    c.parametros = std::move(parametros);
    c.estadoCapturado = ambiente.capturarEstado();
    c.funcao = funcao;
    return c;
}

Closure Closure::capturarBloco(const Ambiente& ambiente,
                               std::vector<std::string> parametros,
                               const std::vector<ast::Comando*>* corpoAst) {
    Closure c;
    c.parametros = std::move(parametros);
    c.estadoCapturado = ambiente.capturarEstado();
    c.corpoAst = corpoAst;
    return c;
}

Valor Closure::operator()(Ambiente& ambiente,
                          const std::vector<Valor>& argumentos) const {
    EscopoGuard escopoLexico(ambiente);

    // 1. Restaura o estado léxico capturado no instante da declaração.
    for (const auto& [nome, valor] : estadoCapturado) {
        ambiente.definirLocal(nome, valor);
    }

    // 2. Parâmetros formais sombreiam variáveis de mesmo nome do estado capturado.
    const std::size_t n = std::min(parametros.size(), argumentos.size());
    for (std::size_t i = 0; i < n; ++i) {
        ambiente.definirLocal(parametros[i], argumentos[i]);
    }

    if (funcao != nullptr) {
        return funcao(ambiente, argumentos);
    }
    return Valor{0.0};
}

}  // namespace mus
