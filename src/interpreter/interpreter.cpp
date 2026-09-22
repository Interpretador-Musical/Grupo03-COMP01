#include "interpreter/interpreter.h"

#include <type_traits>

#include "core/pitch.h"

namespace mus {

bool Interpretador::executar(const std::vector<ast::Comando*>& programa) {
    executarBloco(programa);
    if (falhou_) {
        return false;
    }
    timeline_.sortByStartTime();
    return true;
}

void Interpretador::executarBloco(const std::vector<ast::Comando*>& bloco) {
    for (const ast::Comando* comando : bloco) {
        if (falhou_) {
            return;
        }
        executarComando(*comando);
    }
}

void Interpretador::executarComando(const ast::Comando& comando) {
    if (falhou_) {
        return;
    }
    std::visit(
        [this](const auto& no) {
            using T = std::decay_t<decltype(no)>;
            if constexpr (std::is_same_v<T, ast::ComandoAtribuicao>) {
                executarAtribuicao(no);
            } else if constexpr (std::is_same_v<T, ast::ComandoToca>) {
                executarToca(no);
            } else if constexpr (std::is_same_v<T, ast::ComandoPausa>) {
                executarPausa(no);
            } else if constexpr (std::is_same_v<T, ast::ComandoAndamento>) {
                executarAndamento(no);
            } else if constexpr (std::is_same_v<T, ast::ComandoOitava>) {
                executarOitava(no);
            } else if constexpr (std::is_same_v<T, ast::ComandoVolume>) {
                executarVolume(no);
            } else {
                static_assert(std::is_same_v<T, ast::ComandoRepita>,
                              "novo alternative em ast::Comando sem visitor aqui");
                executarRepita(no);
            }
        },
        comando);
}

void Interpretador::executarAtribuicao(const ast::ComandoAtribuicao& atribuicao) {
    const Valor valor = avaliarExpressao(*atribuicao.valor);
    if (falhou_) {
        return;
    }
    variaveis_[atribuicao.nome] = valor;
}

void Interpretador::executarToca(const ast::ComandoToca& toca) {
    const double midi = paraNumero(avaliarExpressao(*toca.altura));
    if (falhou_) {
        return;
    }
    const double duracaoBeats = paraNumero(avaliarExpressao(*toca.duracao));
    if (falhou_) {
        return;
    }
    timeline_.add(playhead_.play(Note{midi, volumeCorrente_}, duracaoBeats));
}

void Interpretador::executarPausa(const ast::ComandoPausa& pausa) {
    const double duracaoBeats = paraNumero(avaliarExpressao(*pausa.duracao));
    if (falhou_) {
        return;
    }

    // Constrói o evento a partir da posição atual (playHere não avança o
    // cursor) e zera a frequência depois: Note::frequency() nunca daria 0 Hz
    // de verdade para um MIDI 0, então a pausa não pode passar por ela. O
    // evento silencioso entra na Timeline como qualquer outro — é o que
    // permite ao TimelinePlayer (INT-03, #19) tocar tudo sequencialmente sem
    // precisar calcular gaps.
    SoundEvent evento = playhead_.playHere(Note{0.0, 0.0f}, duracaoBeats);
    evento.frequency = 0.0;
    playhead_.advance(duracaoBeats);
    timeline_.add(evento);
}

void Interpretador::executarAndamento(const ast::ComandoAndamento& andamento) {
    const double bpm = paraNumero(avaliarExpressao(*andamento.valor));
    if (falhou_) {
        return;
    }
    if (!playhead_.setBpm(bpm)) {
        falhar("andamento precisa ser positivo, recebeu " + std::to_string(bpm));
    }
}

void Interpretador::executarOitava(const ast::ComandoOitava& oitava) {
    const double valor = paraNumero(avaliarExpressao(*oitava.valor));
    if (falhou_) {
        return;
    }
    oitavaCorrente_ = static_cast<int>(valor);
}

void Interpretador::executarVolume(const ast::ComandoVolume& volume) {
    const double valor = paraNumero(avaliarExpressao(*volume.valor));
    if (falhou_) {
        return;
    }
    volumeCorrente_ = static_cast<float>(valor);
}

void Interpretador::executarRepita(const ast::ComandoRepita& repita) {
    const double vezes = paraNumero(avaliarExpressao(*repita.vezes));
    if (falhou_) {
        return;
    }
    if (vezes < 0.0) {
        falhar("repita precisa de uma quantidade não-negativa, recebeu " +
               std::to_string(vezes));
        return;
    }

    // Revisita os mesmos ponteiros da AST a cada volta — de propósito: é
    // isso que faz um `oitava`/atribuição dentro do corpo valer só a partir
    // da iteração em que rodou, sem resolver nada cedo demais no parse.
    const long quantas = static_cast<long>(vezes);
    for (long i = 0; i < quantas; ++i) {
        if (falhou_) {
            return;
        }
        executarBloco(*repita.corpo);
    }
}

Valor Interpretador::avaliarExpressao(const ast::Expressao& expressao) {
    if (falhou_) {
        return Valor{0.0};
    }

    return std::visit(
        [this](const auto& no) -> Valor {
            using T = std::decay_t<decltype(no)>;

            if constexpr (std::is_same_v<T, ast::ExpressaoLiteralNumero>) {
                return no.valor;
            } else if constexpr (std::is_same_v<T, ast::ExpressaoLiteralBooleano>) {
                return no.valor;
            } else if constexpr (std::is_same_v<T, ast::ExpressaoNota>) {
                // oitava == -1: o programa não escreveu oitava (`toca do`) —
                // usa a corrente. Mesma fórmula de `scanner::analisarNota`,
                // sem duplicar a constante: midi = (oitava+1)*12 + semitom.
                const int oitava = (no.oitava >= 0) ? no.oitava : oitavaCorrente_;
                return static_cast<double>((oitava + 1) * kSemitonesPerOctave +
                                            no.semitom);
            } else if constexpr (std::is_same_v<T, ast::ExpressaoIdentificador>) {
                const auto it = variaveis_.find(no.nome);
                if (it == variaveis_.end()) {
                    falhar("variável '" + no.nome + "' usada antes de ser definida");
                    return Valor{0.0};
                }
                return it->second;
            } else if constexpr (std::is_same_v<T, ast::ExpressaoUnaria>) {
                const Valor operando = avaliarExpressao(*no.operando);
                if (falhou_) {
                    return Valor{0.0};
                }
                if (no.operador == ast::OperadorUnario::Negacao) {
                    return -paraNumero(operando);
                }
                return !paraBooleano(operando);
            } else {
                static_assert(std::is_same_v<T, ast::ExpressaoBinaria>,
                              "novo alternative em ast::Expressao sem visitor aqui");
                const Valor esquerda = avaliarExpressao(*no.esquerda);
                if (falhou_) {
                    return Valor{0.0};
                }
                const Valor direita = avaliarExpressao(*no.direita);
                if (falhou_) {
                    return Valor{0.0};
                }

                switch (no.operador) {
                    case ast::OperadorBinario::Soma:
                        return paraNumero(esquerda) + paraNumero(direita);
                    case ast::OperadorBinario::Subtracao:
                        return paraNumero(esquerda) - paraNumero(direita);
                    case ast::OperadorBinario::Multiplicacao:
                        return paraNumero(esquerda) * paraNumero(direita);
                    case ast::OperadorBinario::Divisao:
                        return paraNumero(esquerda) / paraNumero(direita);
                    case ast::OperadorBinario::Igual:
                        return paraNumero(esquerda) == paraNumero(direita);
                    case ast::OperadorBinario::Diferente:
                        return paraNumero(esquerda) != paraNumero(direita);
                    case ast::OperadorBinario::Menor:
                        return paraNumero(esquerda) < paraNumero(direita);
                    case ast::OperadorBinario::Maior:
                        return paraNumero(esquerda) > paraNumero(direita);
                    case ast::OperadorBinario::MenorOuIgual:
                        return paraNumero(esquerda) <= paraNumero(direita);
                    case ast::OperadorBinario::MaiorOuIgual:
                        return paraNumero(esquerda) >= paraNumero(direita);
                    case ast::OperadorBinario::E:
                        return paraBooleano(esquerda) && paraBooleano(direita);
                    case ast::OperadorBinario::Ou:
                        return paraBooleano(esquerda) || paraBooleano(direita);
                }
                return Valor{0.0};  // inatingível: switch cobre todo OperadorBinario
            }
        },
        expressao);
}

double Interpretador::paraNumero(const Valor& valor) {
    if (std::holds_alternative<double>(valor)) {
        return std::get<double>(valor);
    }
    return std::get<bool>(valor) ? 1.0 : 0.0;
}

bool Interpretador::paraBooleano(const Valor& valor) {
    if (std::holds_alternative<bool>(valor)) {
        return std::get<bool>(valor);
    }
    return std::get<double>(valor) != 0.0;
}

void Interpretador::falhar(const std::string& mensagem) {
    if (!falhou_) {
        falhou_ = true;
        erro_ = mensagem;
    }
}

}  // namespace mus
