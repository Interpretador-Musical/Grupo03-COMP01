#include "interpreter/interpreter.h"
#include "interpreter/ambiente.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <type_traits>
#include <utility>

#include "core/pitch.h"
#include "interpreter/operacoes.h"

namespace mus {

namespace {

// Número para mensagem de erro: `1000000000000`, `10`, `inf`, `nan`. O
// `std::to_string(double)` escreveria `1000000000000.000000`.
//
// O NaN sai sempre como `nan`. O sinal de um NaN não tem significado, mas a
// glibc o imprime: no x86-64 o `0.0 / 0.0` tem o bit de sinal ligado, e no
// Linux a mensagem sairia `-nan`. A libc do macOS imprime `nan` de todo jeito.
std::string legivel(double valor) {
    if (std::isnan(valor)) {
        return "nan";
    }
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.15g", valor);
    return buffer;
}

}  // namespace

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
            } else if constexpr (std::is_same_v<T, ast::ComandoRepita>) {
                executarRepita(no);
            } else {
                static_assert(std::is_same_v<T, ast::ComandoGrupo>,
                              "novo alternative em ast::Comando sem visitor aqui");
                executarGrupo(no);
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
    const Valor altura = avaliarExpressao(*toca.altura);
    if (falhou_) {
        return;
    }

    const Valor duracao = avaliarExpressao(*toca.duracao);
    if (falhou_) {
        return;
    }

    if (!std::holds_alternative<double>(duracao)) {
        falhar("a duracao do toca deve ser numerica");
        return;
    }

    const double duracaoBeats = std::get<double>(duracao);

    if (!playhead_.canAdvance(duracaoBeats)) {
        falhar("duracao de nota invalida: o valor deve ser finito, nao negativo e representavel");
        return;
    }

    // INT-05: reproduz um sample previamente carregado.
    if (std::holds_alternative<std::string>(altura)) {
        const std::string& nome = std::get<std::string>(altura);

        if (sampleBank_ == nullptr) {
            falhar("banco de samples nao configurado");
            return;
        }

        const auto id = sampleBank_->findId(nome);
        if (!id.has_value()) {
            falhar("sample '" + nome + "' nao encontrado no banco");
            return;
        }

        SoundEvent evento = playhead_.playHere(
            Note{0.0, volumeCorrente_}, duracaoBeats);

        evento.type = SoundEventType::Sample;
        evento.sampleId = *id;
        evento.frequency = 0.0;

        registrarEvento(evento);
        if (falhou_) {
            return;
        }
        playhead_.advance(duracaoBeats);
        return;
    }

    // Caminho original: notas e numeros MIDI.
    const double midi = paraNumero(altura);
    const Note nota{midi, volumeCorrente_};
    const double frequencia = nota.frequency();
    if (!std::isfinite(frequencia) || frequencia <= 0.0) {
        falhar("altura de nota invalida: a frequencia deve ser finita e positiva");
        return;
    }

    registrarEvento(playhead_.play(nota, duracaoBeats));
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
    registrarEvento(evento);
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
    // As oitavas que têm notas MIDI: a -1 começa no MIDI 0 (dó -1), e a 9
    // chega ao 127 (sol 9). O valor é truncado, e o intervalo aberto (-2, 10)
    // é exatamente o que trunca para -1 a 9. Fora dela, o `static_cast` seria comportamento indefinido (inf,
    // NaN) ou estouraria a conta `(oitava + 1) * 12` das notas (#124). NaN
    // falha nas duas comparações.
    if (!(valor > -2.0 && valor < 10.0)) {
        falhar("oitava precisa estar entre -1 e 9, recebeu " + legivel(valor));
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
    // Tudo antes do `static_cast`, que com inf, NaN ou valor fora da faixa
    // do inteiro é comportamento indefinido (#124). NaN precisa da própria
    // checagem: ele não é menor que zero, e passaria pela de baixo.
    if (!std::isfinite(vezes)) {
        falhar("repita precisa de uma quantidade finita, recebeu " + legivel(vezes));
        return;
    }
    if (vezes < 0.0) {
        falhar("repita precisa de uma quantidade não-negativa, recebeu " +
               std::to_string(vezes));
        return;
    }
    // Comparado em double, antes de truncar: `repita 1e12 vezes { }` é
    // recusado aqui, sem rodar volta nenhuma.
    if (vezes >= static_cast<double>(limites_.voltas) + 1.0) {
        falhar("repita pede " + legivel(std::floor(vezes)) +
               " voltas, acima do limite de " + std::to_string(limites_.voltas) +
               " por programa");
        return;
    }

    const auto quantas = static_cast<std::uint64_t>(vezes);
    for (std::uint64_t i = 0; i < quantas; ++i) {
        if (falhou_) {
            return;
        }
        // O contador é do programa inteiro, e não deste `repita`: aninhados,
        // cada um passa na checagem de cima, mas as voltas se multiplicam
        // (#125).
        if (++voltasExecutadas_ > limites_.voltas) {
            falhar("o programa passa do limite de " + std::to_string(limites_.voltas) +
                   " voltas de repita");
            return;
        }
        EscopoGuard escopoIteracao(ambiente_);
        executarBloco(*repita.corpo);
    }
}

void Interpretador::executarGrupo(const ast::ComandoGrupo& grupo) {
    // Toda trilha começa do mesmo estado: o cursor, a oitava e o volume de
    // quando o grupo começou. O que uma trilha muda nisso fica com ela.
    const Playhead entrada = playhead_;
    const int oitavaDeEntrada = oitavaCorrente_;
    const float volumeDeEntrada = volumeCorrente_;

    // O cursor sai do grupo onde a trilha mais longa terminou (no empate, a
    // primeira). Copiar o Playhead inteiro, e não só os segundos, mantém o
    // tempo contado em tempos (MAT-04, #22): como a análise semântica proíbe
    // `andamento` dentro de grupo, todas as trilhas têm o BPM e a âncora da
    // entrada, e comparar `now()` é o mesmo que comparar tempos.
    Playhead maisLonga = entrada;
    for (const std::vector<ast::Comando*>* trilha : *grupo.trilhas) {
        if (falhou_) {
            return;
        }
        playhead_ = entrada;
        oitavaCorrente_ = oitavaDeEntrada;
        volumeCorrente_ = volumeDeEntrada;

        executarBloco(*trilha);
        if (playhead_.now() > maisLonga.now()) {
            maisLonga = playhead_;
        }
    }

    // Os eventos das trilhas entram na Timeline uma trilha depois da outra;
    // a ordenação estável do fim de `executar` os intercala por início, e os
    // simultâneos ficam na ordem das trilhas.
    playhead_ = maisLonga;
    oitavaCorrente_ = oitavaDeEntrada;
    volumeCorrente_ = volumeDeEntrada;
}

void Interpretador::registrarEvento(const SoundEvent& evento) {
    // Antes do `push_back`: o programa que passaria do teto é recusado sem
    // alocar além dele (#125).
    if (timeline_.size() >= limites_.eventos) {
        falhar("o programa passa do limite de " + std::to_string(limites_.eventos) +
               " eventos na Timeline");
        return;
    }
    timeline_.add(evento);
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
            } else if constexpr (std::is_same_v<T, ast::ExpressaoLiteralString>) {
                return no.valor;
            } else if constexpr (std::is_same_v<T, ast::ExpressaoNota>) {
                const int oitava = (no.oitava >= 0) ? no.oitava : oitavaCorrente_;
                return midiDaNota(no.semitom, oitava);
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
                return aplicarUnario(no.operador, operando);
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

                // O significado dos operadores mora em operacoes.h, junto com
                // a dobra de constantes (COMP-D, #110), para os dois nunca
                // divergirem.
                std::optional<Valor> resultado =
                    aplicarBinario(no.operador, esquerda, direita);
                if (!resultado.has_value()) {
                    falhar("operacao invalida entre strings");
                    return Valor{0.0};
                }
                return std::move(*resultado);
            }
        },
        expressao);
}

double Interpretador::paraNumero(const Valor& valor) {
    return mus::paraNumero(valor);
}

bool Interpretador::paraBooleano(const Valor& valor) {
    return mus::paraBooleano(valor);
}

void Interpretador::falhar(const std::string& mensagem) {
    if (!falhou_) {
        falhou_ = true;
        erro_ = mensagem;
    }
}

}  // namespace mus
