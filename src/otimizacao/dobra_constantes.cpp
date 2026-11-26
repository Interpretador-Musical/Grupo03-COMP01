#include "otimizacao/dobra_constantes.h"

#include <algorithm>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

#include "interpreter/ambiente.h"
#include "interpreter/operacoes.h"

namespace mus {
namespace otimizacao {
namespace {

int contar(const ast::Expressao* e) {
    if (e == nullptr) {
        return 0;
    }
    if (const auto* u = std::get_if<ast::ExpressaoUnaria>(e)) {
        return 1 + contar(u->operando);
    }
    if (const auto* b = std::get_if<ast::ExpressaoBinaria>(e)) {
        return 1 + contar(b->esquerda) + contar(b->direita);
    }
    return 1;
}

int contar(const std::vector<ast::Comando*>* bloco);

int contar(const ast::Comando* comando) {
    return std::visit(
        [](const auto& no) -> int {
            using T = std::decay_t<decltype(no)>;
            if constexpr (std::is_same_v<T, ast::ComandoAtribuicao>) {
                return contar(no.valor);
            } else if constexpr (std::is_same_v<T, ast::ComandoToca>) {
                return contar(no.altura) + contar(no.duracao);
            } else if constexpr (std::is_same_v<T, ast::ComandoPausa>) {
                return contar(no.duracao);
            } else if constexpr (std::is_same_v<T, ast::ComandoAndamento> ||
                                 std::is_same_v<T, ast::ComandoOitava> ||
                                 std::is_same_v<T, ast::ComandoVolume>) {
                return contar(no.valor);
            } else if constexpr (std::is_same_v<T, ast::ComandoRepita>) {
                return contar(no.vezes) + contar(no.corpo);
            } else {
                static_assert(std::is_same_v<T, ast::ComandoGrupo>,
                              "novo alternative em ast::Comando sem contagem aqui");
                int total = 0;
                if (no.trilhas != nullptr) {
                    for (const auto* trilha : *no.trilhas) {
                        total += contar(trilha);
                    }
                }
                return total;
            }
        },
        *comando);
}

int contar(const std::vector<ast::Comando*>* bloco) {
    int total = 0;
    if (bloco != nullptr) {
        for (const ast::Comando* comando : *bloco) {
            total += contar(comando);
        }
    }
    return total;
}

// Troca o nó, no lugar, por um literal com `valor`, e diz se trocou. O nó é
// alterado em vez de substituído porque o pai guarda um ponteiro para ele:
// assim ninguém precisa ser reapontado. Os filhos antigos ficam abandonados na
// arena da árvore e somem com ela (MAT-10, #55); nada é liberado aqui.
//
// Um resultado string não é trocado: o literal guarda só uma vista sobre a
// arena, e a dobra não tem a arena para copiar o texto. Hoje nenhuma operação
// dá string (entre strings só há `==` e `!=`, que dão booleano), então isto
// nunca deixa de dobrar nada; se um dia houver, o nó fica como está e é
// avaliado na execução, com o mesmo resultado.
bool trocarPorLiteral(ast::Expressao& e, const Valor& valor) {
    if (const auto* numero = std::get_if<double>(&valor)) {
        e.emplace<ast::ExpressaoLiteralNumero>(ast::ExpressaoLiteralNumero{*numero});
    } else if (const auto* booleano = std::get_if<bool>(&valor)) {
        e.emplace<ast::ExpressaoLiteralBooleano>(ast::ExpressaoLiteralBooleano{*booleano});
    } else {
        return false;
    }
    e.profundidade = 1;
    return true;
}

class Dobrador {
public:
    int dobradas() const { return dobradas_; }

    // Dobra a subárvore e devolve o seu valor, se ela for constante.
    std::optional<Valor> dobrar(ast::Expressao* e) {
        if (e == nullptr) {
            return std::nullopt;
        }
        return std::visit(
            [this, e](auto& no) -> std::optional<Valor> {
                using T = std::decay_t<decltype(no)>;
                if constexpr (std::is_same_v<T, ast::ExpressaoLiteralNumero> ||
                              std::is_same_v<T, ast::ExpressaoLiteralBooleano>) {
                    return Valor{no.valor};
                } else if constexpr (std::is_same_v<T, ast::ExpressaoLiteralString>) {
                    return Valor{std::string(no.valor)};
                } else if constexpr (std::is_same_v<T, ast::ExpressaoNota>) {
                    // Constante só com oitava escrita. A folha fica como está:
                    // avaliar uma nota já é tão barato quanto um literal.
                    if (no.oitava < 0) {
                        return std::nullopt;
                    }
                    return Valor{midiDaNota(no.semitom, no.oitava)};
                } else if constexpr (std::is_same_v<T, ast::ExpressaoIdentificador>) {
                    return std::nullopt;
                } else if constexpr (std::is_same_v<T, ast::ExpressaoUnaria>) {
                    std::optional<Valor> operando = dobrar(no.operando);
                    if (!operando.has_value()) {
                        e->profundidade = 1 + no.operando->profundidade;
                        return std::nullopt;
                    }
                    Valor resultado = aplicarUnario(no.operador, *operando);
                    if (!trocarPorLiteral(*e, resultado)) {
                        e->profundidade = 1 + no.operando->profundidade;
                        return std::nullopt;
                    }
                    // Daqui em diante `no` não existe mais: virou o literal.
                    ++dobradas_;
                    return resultado;
                } else {
                    static_assert(std::is_same_v<T, ast::ExpressaoBinaria>,
                                  "novo alternative em ast::Expressao sem dobra aqui");
                    // Os dois lados são sempre dobrados, mesmo que um não seja
                    // constante: `x * (2 + 3)` ainda vira `x * 5`.
                    std::optional<Valor> esquerda = dobrar(no.esquerda);
                    std::optional<Valor> direita = dobrar(no.direita);
                    if (esquerda.has_value() && direita.has_value()) {
                        std::optional<Valor> resultado =
                            aplicarBinario(no.operador, *esquerda, *direita);
                        if (resultado.has_value() && trocarPorLiteral(*e, *resultado)) {
                            // Daqui em diante `no` não existe mais: virou o literal.
                            ++dobradas_;
                            return resultado;
                        }
                    }
                    e->profundidade =
                        1 + std::max(no.esquerda->profundidade, no.direita->profundidade);
                    return std::nullopt;
                }
            },
            static_cast<ast::Expressao::variant&>(*e));
    }

    // Devolve a profundidade do comando, recalculada depois da dobra.
    int dobrar(ast::Comando* comando) {
        const int filho = std::visit(
            [this](auto& no) -> int {
                using T = std::decay_t<decltype(no)>;
                if constexpr (std::is_same_v<T, ast::ComandoAtribuicao>) {
                    return profundidadeDepois(no.valor);
                } else if constexpr (std::is_same_v<T, ast::ComandoToca>) {
                    return std::max(profundidadeDepois(no.altura),
                                    profundidadeDepois(no.duracao));
                } else if constexpr (std::is_same_v<T, ast::ComandoPausa>) {
                    return profundidadeDepois(no.duracao);
                } else if constexpr (std::is_same_v<T, ast::ComandoAndamento> ||
                                     std::is_same_v<T, ast::ComandoOitava> ||
                                     std::is_same_v<T, ast::ComandoVolume>) {
                    return profundidadeDepois(no.valor);
                } else if constexpr (std::is_same_v<T, ast::ComandoRepita>) {
                    return std::max(profundidadeDepois(no.vezes), dobrar(no.corpo));
                } else {
                    static_assert(std::is_same_v<T, ast::ComandoGrupo>,
                                  "novo alternative em ast::Comando sem dobra aqui");
                    int maior = 0;
                    if (no.trilhas != nullptr) {
                        for (const auto* trilha : *no.trilhas) {
                            maior = std::max(maior, dobrar(trilha));
                        }
                    }
                    return maior;
                }
            },
            static_cast<ast::Comando::variant&>(*comando));
        comando->profundidade = 1 + filho;
        return comando->profundidade;
    }

    // Profundidade do comando mais fundo do bloco. O vetor é const: a dobra
    // altera os comandos no lugar, mas nunca quais comandos o bloco tem.
    int dobrar(const std::vector<ast::Comando*>* bloco) {
        int maior = 0;
        if (bloco != nullptr) {
            for (ast::Comando* comando : *bloco) {
                maior = std::max(maior, dobrar(comando));
            }
        }
        return maior;
    }

private:
    int profundidadeDepois(ast::Expressao* e) {
        if (e == nullptr) {
            return 0;
        }
        dobrar(e);
        return e->profundidade;
    }

    int dobradas_ = 0;
};

}  // namespace

int contarNosDeExpressao(const std::vector<ast::Comando*>& programa) {
    return contar(&programa);
}

ResultadoDobra dobrarConstantes(const std::vector<ast::Comando*>& programa) {
    ResultadoDobra resultado;
    resultado.nosAntes = contarNosDeExpressao(programa);

    Dobrador dobrador;
    dobrador.dobrar(&programa);

    resultado.expressoesDobradas = dobrador.dobradas();
    resultado.nosDepois = contarNosDeExpressao(programa);
    return resultado;
}

}  // namespace otimizacao
}  // namespace mus
