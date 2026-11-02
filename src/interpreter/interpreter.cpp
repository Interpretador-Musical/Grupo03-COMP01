#include "interpreter/interpreter.h"
#include "interpreter/ambiente.h"
#include <algorithm>
#include <cmath>
#include <type_traits>
#include <utility>

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

void Interpretador::declararClosure(const std::string& nome,
                                    std::vector<std::string> parametros,
                                    const std::vector<ast::Comando*>* corpoAst) {
    closures_[nome] =
        Closure::capturarBloco(ambiente_, std::move(parametros), corpoAst);
}

bool Interpretador::invocarClosure(const std::string& nome,
                                   const std::vector<Valor>& argumentos) {
    const auto it = closures_.find(nome);
    if (it == closures_.end()) {
        falhar("função '" + nome + "' chamada antes de ser declarada");
        return false;
    }

    const Closure& closure = it->second;
    if (argumentos.size() != closure.parametros.size()) {
        falhar("função '" + nome + "' esperava " +
               std::to_string(closure.parametros.size()) +
               " argumento(s), recebeu " + std::to_string(argumentos.size()));
        return false;
    }

    // Escopo léxico: o corpo roda num ambiente novo, montado só com o que foi
    // capturado na declaração. O do chamador fica de fora, então nada criado
    // depois da declaração vaza para dentro da função.
    Ambiente isolado;
    for (const auto& [varNome, varValor] : closure.estadoCapturado) {
        isolado.definirLocal(varNome, varValor);
    }
    isolado.abrirEscopo();  // parâmetros sombreiam o capturado
    for (std::size_t i = 0; i < closure.parametros.size(); ++i) {
        isolado.definirLocal(closure.parametros[i], argumentos[i]);
    }

    std::swap(ambiente_, isolado);  // entra no ambiente da função
    if (closure.corpoAst != nullptr) {
        executarBloco(*closure.corpoAst);
    }
    std::swap(ambiente_, isolado);  // sai: o do chamador volta (não há exceções)

    if (!falhou_) {
        timeline_.sortByStartTime();
    }
    return !falhou_;
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
    ambiente_.atribuir(atribuicao.nome, valor);
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
    // 0 Hz é reservado para pausa no IR, então uma altura que dê 0 Hz (ou
    // inf/NaN) não pode virar evento sonoro.
    const Note nota{midi, volumeCorrente_};
    const double frequencia = nota.frequency();
    if (!std::isfinite(frequencia) || frequencia <= 0.0) {
        falhar("altura de nota invalida: a frequencia deve ser finita e positiva");
        return;
    }
    if (!playhead_.canAdvance(duracaoBeats)) {
        falhar("duracao de nota invalida: o valor deve ser finito, nao negativo e representavel");
        return;
    }
    timeline_.add(playhead_.play(nota, duracaoBeats));
}

void Interpretador::executarPausa(const ast::ComandoPausa& pausa) {
    const double duracaoBeats = paraNumero(avaliarExpressao(*pausa.duracao));
    if (falhou_) {
        return;
    }
    if (!playhead_.canAdvance(duracaoBeats)) {
        falhar("duracao de pausa invalida: o valor deve ser finito, nao negativo e representavel");
        return;
    }

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
        falhar("andamento invalido: BPM deve ser positivo, finito e representavel; recebeu " +
               std::to_string(bpm));
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
    if (!(valor >= 0.0 && valor <= 1.0)) {
        falhar("volume precisa estar entre 0 e 1, recebeu " + std::to_string(valor));
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

        const long quantas = static_cast<long>(vezes);
    for (long i = 0; i < quantas; ++i) {
        if (falhou_) {
            return;
        }
        EscopoGuard escopoIteracao(ambiente_);
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
                const int oitava = (no.oitava >= 0) ? no.oitava : oitavaCorrente_;
                return static_cast<double>((oitava + 1) * kSemitonesPerOctave +
                                            no.semitom);
            } else if constexpr (std::is_same_v<T, ast::ExpressaoIdentificador>) {
                const Valor* encontrado = ambiente_.buscar(no.nome);
                if (encontrado == nullptr) {
                    falhar("variável '" + no.nome + "' usada antes de ser definida");
                    return Valor{0.0};
                }
                return *encontrado;
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
                    case ast::OperadorBinario::ELogico:
                        return paraBooleano(esquerda) && paraBooleano(direita);
                    case ast::OperadorBinario::OuLogico:
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
