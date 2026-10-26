#include "semantic/analisador_semantico.h"

#include <type_traits>

namespace mus {
namespace semantica {

namespace {

const char* simboloOperador(ast::OperadorBinario operador) {
    switch (operador) {
        case ast::OperadorBinario::Soma:
            return "+";
        case ast::OperadorBinario::Subtracao:
            return "-";
        case ast::OperadorBinario::Multiplicacao:
            return "*";
        case ast::OperadorBinario::Divisao:
            return "/";
        case ast::OperadorBinario::Igual:
            return "==";
        case ast::OperadorBinario::Diferente:
            return "!=";
        case ast::OperadorBinario::Menor:
            return "<";
        case ast::OperadorBinario::Maior:
            return ">";
        case ast::OperadorBinario::MenorOuIgual:
            return "<=";
        case ast::OperadorBinario::MaiorOuIgual:
            return ">=";
        case ast::OperadorBinario::ELogico:
            return "e";
        case ast::OperadorBinario::OuLogico:
            return "ou";
    }
    return "?";  // inatingível: switch cobre todo OperadorBinario
}

}  // namespace

std::string ErroSemantico::formatado() const {
    return "Linha " + std::to_string(posicao.linha) + ", Coluna " +
           std::to_string(posicao.coluna) + ": " + mensagem;
}

bool AnalisadorSemantico::analisar(const std::vector<ast::Comando*>& programa) {
    analisarBloco(programa);
    return erros_.empty();
}

void AnalisadorSemantico::analisarBloco(const std::vector<ast::Comando*>& bloco) {
    for (const ast::Comando* comando : bloco) {
        analisarComando(*comando);
    }
}

void AnalisadorSemantico::analisarComando(const ast::Comando& comando) {
    std::visit(
        [this](const auto& no) {
            using T = std::decay_t<decltype(no)>;
            if constexpr (std::is_same_v<T, ast::ComandoAtribuicao>) {
                analisarAtribuicao(no);
            } else if constexpr (std::is_same_v<T, ast::ComandoToca>) {
                // Altura aceita número além de nota: é um MIDI cru
                // (`toca 60 por 1`), que o interpretador já toca.
                const Tipo altura = tipoDe(*no.altura);
                if (altura != Tipo::Nota && altura != Tipo::Numero &&
                    altura != Tipo::Erro) {
                    reportar(no.altura->posicao,
                             std::string("toca espera nota ou numero como altura, recebeu ") +
                                 nomeTipo(altura));
                }
                exigirNumero(*no.duracao, "a duração do toca");
            } else if constexpr (std::is_same_v<T, ast::ComandoPausa>) {
                exigirNumero(*no.duracao, "a duração da pausa");
            } else if constexpr (std::is_same_v<T, ast::ComandoAndamento>) {
                exigirNumero(*no.valor, "andamento");
            } else if constexpr (std::is_same_v<T, ast::ComandoOitava>) {
                exigirNumero(*no.valor, "oitava");
            } else if constexpr (std::is_same_v<T, ast::ComandoVolume>) {
                exigirNumero(*no.valor, "volume");
            } else {
                static_assert(std::is_same_v<T, ast::ComandoRepita>,
                              "novo alternative em ast::Comando sem visitor aqui");
                exigirNumero(*no.vezes, "repita");
                tabela_.abrirEscopo();
                analisarBloco(*no.corpo);
                tabela_.fecharEscopo();
            }
        },
        comando);
}

void AnalisadorSemantico::analisarAtribuicao(const ast::ComandoAtribuicao& atribuicao) {
    // O valor é analisado antes de declarar: em `x = x + 1` com `x` novo, o
    // `x` da direita ainda não existe e precisa ser acusado.
    const Tipo tipo = tipoDe(*atribuicao.valor);

    const std::size_t indice = tabela_.buscar(atribuicao.nome);
    if (indice == TabelaSimbolos::kNaoEncontrado) {
        // Declara mesmo com tipo Erro: assim os usos seguintes do nome não
        // viram um "não declarado" a mais por cima do erro que já foi dado.
        tabela_.declarar(atribuicao.nome, tipo, atribuicao.posicao);
        return;
    }

    tabela_.registrarUso(indice, atribuicao.posicao);

    const Tipo declarado = tabela_.simbolo(indice).tipo;
    if (tipo != declarado && tipo != Tipo::Erro && declarado != Tipo::Erro) {
        reportar(atribuicao.posicao,
                 "redeclaração inválida: '" + atribuicao.nome + "' é " +
                     nomeTipo(declarado) + ", recebeu " + nomeTipo(tipo));
    }
}

Tipo AnalisadorSemantico::tipoDe(const ast::Expressao& expressao) {
    return std::visit(
        [this, &expressao](const auto& no) -> Tipo {
            using T = std::decay_t<decltype(no)>;

            if constexpr (std::is_same_v<T, ast::ExpressaoLiteralNumero>) {
                return Tipo::Numero;
            } else if constexpr (std::is_same_v<T, ast::ExpressaoLiteralBooleano>) {
                return Tipo::Booleano;
            } else if constexpr (std::is_same_v<T, ast::ExpressaoNota>) {
                return Tipo::Nota;
            } else if constexpr (std::is_same_v<T, ast::ExpressaoIdentificador>) {
                const std::size_t indice = tabela_.buscar(no.nome);
                if (indice == TabelaSimbolos::kNaoEncontrado) {
                    reportar(expressao.posicao,
                             "'" + no.nome +
                                 "' usado antes de ser declarado ou fora de escopo");
                    return Tipo::Erro;
                }
                tabela_.registrarUso(indice, expressao.posicao);
                return tabela_.simbolo(indice).tipo;
            } else if constexpr (std::is_same_v<T, ast::ExpressaoUnaria>) {
                return tipoUnaria(no, expressao.posicao);
            } else {
                static_assert(std::is_same_v<T, ast::ExpressaoBinaria>,
                              "novo alternative em ast::Expressao sem visitor aqui");
                return tipoBinaria(no, expressao.posicao);
            }
        },
        expressao);
}

Tipo AnalisadorSemantico::tipoBinaria(const ast::ExpressaoBinaria& binaria,
                                      ast::Posicao posicao) {
    const Tipo esquerda = tipoDe(*binaria.esquerda);
    const Tipo direita = tipoDe(*binaria.direita);
    if (esquerda == Tipo::Erro || direita == Tipo::Erro) {
        return Tipo::Erro;
    }

    const bool numeros = esquerda == Tipo::Numero && direita == Tipo::Numero;
    const bool notas = esquerda == Tipo::Nota && direita == Tipo::Nota;

    Tipo resultado = Tipo::Erro;
    switch (binaria.operador) {
        case ast::OperadorBinario::Soma:
            // Nota + número transpõe: `do4 + 7` é sol 4.
            if (numeros) {
                resultado = Tipo::Numero;
            } else if ((esquerda == Tipo::Nota && direita == Tipo::Numero) ||
                       (esquerda == Tipo::Numero && direita == Tipo::Nota)) {
                resultado = Tipo::Nota;
            }
            break;
        case ast::OperadorBinario::Subtracao:
            // `la4 - 2` transpõe para baixo; `sol4 - do4` é o intervalo em
            // semitons. Já `2 - la4` não tem leitura musical.
            if (numeros || notas) {
                resultado = Tipo::Numero;
            } else if (esquerda == Tipo::Nota && direita == Tipo::Numero) {
                resultado = Tipo::Nota;
            }
            break;
        case ast::OperadorBinario::Multiplicacao:
        case ast::OperadorBinario::Divisao:
            if (numeros) {
                resultado = Tipo::Numero;
            }
            break;
        case ast::OperadorBinario::Igual:
        case ast::OperadorBinario::Diferente:
            if (esquerda == direita) {
                resultado = Tipo::Booleano;
            }
            break;
        case ast::OperadorBinario::Menor:
        case ast::OperadorBinario::Maior:
        case ast::OperadorBinario::MenorOuIgual:
        case ast::OperadorBinario::MaiorOuIgual:
            if (numeros || notas) {
                resultado = Tipo::Booleano;
            }
            break;
        case ast::OperadorBinario::ELogico:
        case ast::OperadorBinario::OuLogico:
            if (esquerda == Tipo::Booleano && direita == Tipo::Booleano) {
                resultado = Tipo::Booleano;
            }
            break;
    }

    if (resultado == Tipo::Erro) {
        reportar(posicao, std::string("operação inválida: ") + nomeTipo(esquerda) +
                              " " + simboloOperador(binaria.operador) + " " +
                              nomeTipo(direita));
    }
    return resultado;
}

Tipo AnalisadorSemantico::tipoUnaria(const ast::ExpressaoUnaria& unaria,
                                     ast::Posicao posicao) {
    const Tipo operando = tipoDe(*unaria.operando);
    if (operando == Tipo::Erro) {
        return Tipo::Erro;
    }

    if (unaria.operador == ast::OperadorUnario::Negacao) {
        if (operando == Tipo::Numero) {
            return Tipo::Numero;
        }
        reportar(posicao, std::string("operação inválida: -") + nomeTipo(operando));
        return Tipo::Erro;
    }

    if (operando == Tipo::Booleano) {
        return Tipo::Booleano;
    }
    reportar(posicao, std::string("operação inválida: nao ") + nomeTipo(operando));
    return Tipo::Erro;
}

void AnalisadorSemantico::exigirNumero(const ast::Expressao& expressao,
                                       const char* contexto) {
    const Tipo tipo = tipoDe(expressao);
    if (tipo != Tipo::Numero && tipo != Tipo::Erro) {
        reportar(expressao.posicao, std::string(contexto) + " espera numero, recebeu " +
                                        nomeTipo(tipo));
    }
}

void AnalisadorSemantico::reportar(ast::Posicao posicao, const std::string& mensagem) {
    erros_.push_back(ErroSemantico{posicao, mensagem});
}

}  // namespace semantica
}  // namespace mus
