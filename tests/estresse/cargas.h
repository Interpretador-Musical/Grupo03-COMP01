#ifndef MUS_TESTS_ESTRESSE_CARGAS_H
#define MUS_TESTS_ESTRESSE_CARGAS_H

// Cargas do teste de estresse do pipeline batch (INT-07, #43).
//
// Cada carga é um programa `.mus` gerado com o número de eventos e a duração
// conhecidos de antemão, por fórmula fechada, e não perguntando ao
// interpretador. É o que permite afirmar que nenhum evento sumiu: o esperado
// não depende do código que está sendo testado.
//
// O cabeçalho é usado por dois consumidores, para que os dois falem das mesmas
// cargas:
// - `tests/test_estresse.cpp` afirma propriedades (contagem, duração, saída
//   limpa) e roda no CTest e no CI;
// - `tests/estresse/medir_estresse.cpp` mede o tempo de cada fase e imprime a
//   tabela de `docs/estresse.md`. Tempo não é afirmado em teste nenhum: o CI
//   roda em máquina compartilhada e sob sanitizer.
//
// Nada aqui pressupõe padrão infinito nem edição em tempo real: todo programa
// é finito e é compilado inteiro antes de qualquer amostra ser gerada.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "audio/audio_engine.h"
#include "core/playhead.h"
#include "core/ring_buffer.h"
#include "core/sound_event.h"
#include "core/timeline_queue.h"
#include "frontend/frontend.h"
#include "interpreter/interpreter.h"
#include "semantic/analisador_semantico.h"

namespace mus {
namespace estresse {

// Abaixo disto o envelope fixo do motor de áudio (5 ms de ataque e 5 ms de
// release, em `AudioEngine::renderRealTime`) não completa o ataque, e a nota
// sai atenuada ou muda. É a fronteira dos cenários aceitos; o que fica abaixo
// dela está medido em `docs/estresse.md` e tem issue própria.
constexpr double kMenorNotaAceitaSegundos = 0.010;

struct Carga {
    std::string nome;
    std::string fonte;
    std::size_t eventos = 0;        // notas + pausas
    std::size_t notas = 0;          // eventos com som
    double duracaoSegundos = 0.0;   // fim do último evento
    double bpm = 0.0;               // andamento principal, para a tabela
    double menorNotaSegundos = 0.0; // nota mais curta do programa
};

// --- Geradores -------------------------------------------------------------

// Grade de andamento × densidade: o mesmo compasso (duas notas e uma pausa,
// todas de `figura` tempos) repetido até dar `segundosAlvo`. Mudar o BPM ou a
// figura muda a densidade em eventos por segundo sem mudar a forma do programa.
inline Carga cargaGrade(double bpm, double figura, double segundosAlvo) {
    const double segundosPorCompasso = 3.0 * figura * 60.0 / bpm;
    const auto repeticoes = static_cast<std::size_t>(
        std::max(1.0, std::round(segundosAlvo / segundosPorCompasso)));

    std::ostringstream fonte;
    fonte << "andamento " << bpm << "\n"
          << "repita " << repeticoes << " vezes {\n"
          << "    toca do4 por " << figura << "\n"
          << "    toca sol4 por " << figura << "\n"
          << "    pausa por " << figura << "\n"
          << "}\n";

    std::ostringstream nome;
    nome << "grade " << bpm << " BPM, figura " << figura;

    Carga carga;
    carga.nome = nome.str();
    carga.fonte = fonte.str();
    carga.eventos = 3 * repeticoes;
    carga.notas = 2 * repeticoes;
    carga.duracaoSegundos = static_cast<double>(repeticoes) * segundosPorCompasso;
    carga.bpm = bpm;
    carga.menorNotaSegundos = figura * 60.0 / bpm;
    return carga;
}

// Volume grande com programa pequeno: dois `repita` aninhados. Exercita o
// interpretador, a Timeline e a fila, não o lexer.
inline Carga cargaAninhada(double bpm, std::size_t externo, std::size_t interno,
                           double figura) {
    std::ostringstream fonte;
    fonte << "andamento " << bpm << "\n"
          << "repita " << externo << " vezes {\n"
          << "    repita " << interno << " vezes {\n"
          << "        toca do4 por " << figura << "\n"
          << "    }\n"
          << "}\n";

    std::ostringstream nome;
    nome << "aninhada " << externo << " x " << interno;

    Carga carga;
    carga.nome = nome.str();
    carga.fonte = fonte.str();
    carga.eventos = externo * interno;
    carga.notas = carga.eventos;
    carga.duracaoSegundos = static_cast<double>(carga.eventos) * figura * 60.0 / bpm;
    carga.bpm = bpm;
    carga.menorNotaSegundos = figura * 60.0 / bpm;
    return carga;
}

// Volume grande com programa grande: uma linha por nota, sem `repita`.
// Exercita o lexer, o parser e a análise semântica, que nas outras cargas
// veem só meia dúzia de linhas.
inline Carga cargaPlana(double bpm, std::size_t linhas, double figura) {
    static const char* const kNotas[] = {"do", "re", "mi", "fa", "sol", "la", "si"};

    std::ostringstream fonte;
    fonte << "andamento " << bpm << "\n";
    for (std::size_t i = 0; i < linhas; ++i) {
        fonte << "toca " << kNotas[i % 7] << (3 + i % 3) << " por " << figura << "\n";
    }

    std::ostringstream nome;
    nome << "plana " << linhas << " linhas";

    Carga carga;
    carga.nome = nome.str();
    carga.fonte = fonte.str();
    carga.eventos = linhas;
    carga.notas = linhas;
    carga.duracaoSegundos = static_cast<double>(linhas) * figura * 60.0 / bpm;
    carga.bpm = bpm;
    carga.menorNotaSegundos = figura * 60.0 / bpm;
    return carga;
}

// Tudo o que a linguagem tem, junto e repetido: variável, transposição por
// expressão, `oitava`, `volume`, troca de andamento dentro do laço e três
// níveis de `repita`. O andamento alterna entre 600 e 1200 BPM a cada volta
// externa.
//
// Por volta externa (o `andamento` vale para tudo o que vem depois dele):
//   600 BPM : 4 × (do + mi + pausa)   = 12 eventos, 12 × 0.5 tempo  = 0.6 s
//   1200 BPM: 4 × (sol + la + pausa)  = 12 eventos, 12 × 0.5 tempo  = 0.3 s
//             + 1 nota de 1 tempo     =  1 evento                   = 0.05 s
// A nota mais curta é de 0.5 tempo a 1200 BPM: 25 ms.
inline Carga cargaMista(std::size_t voltas) {
    std::ostringstream fonte;
    fonte << "base = do4\n"
          << "figura = 0.5\n"
          << "repita " << voltas << " vezes {\n"
          << "    andamento 600\n"
          << "    volume 0.8\n"
          << "    repita 2 vezes {\n"
          << "        repita 2 vezes {\n"
          << "            toca base por figura\n"
          << "            toca base + 4 por figura\n"
          << "            pausa por figura\n"
          << "        }\n"
          << "    }\n"
          << "    andamento 600 * 2\n"
          << "    oitava 5\n"
          << "    volume 0.5\n"
          << "    repita 4 vezes {\n"
          << "        toca sol por figura\n"
          << "        toca la por figura\n"
          << "        pausa por figura\n"
          << "    }\n"
          << "    toca base - 12 por figura * 2\n"
          << "    oitava 4\n"
          << "}\n";

    std::ostringstream nome;
    nome << "mista " << voltas << " voltas";

    Carga carga;
    carga.nome = nome.str();
    carga.fonte = fonte.str();
    carga.eventos = 25 * voltas;
    carga.notas = 17 * voltas;
    carga.duracaoSegundos = static_cast<double>(voltas) * (0.6 + 0.3 + 0.05);
    carga.bpm = 1200.0;
    carga.menorNotaSegundos = 0.025;
    return carga;
}

// --- Pipeline ----------------------------------------------------------------

using Relogio = std::chrono::steady_clock;

inline double segundosDesde(Relogio::time_point inicio) {
    return std::chrono::duration<double>(Relogio::now() - inicio).count();
}

// As mesmas fases, na mesma ordem, que o `main.cpp` percorre antes de abrir o
// dispositivo de áudio.
struct Compilacao {
    bool ok = false;
    std::string erro;
    Timeline timeline;
    double segundosFrontend = 0.0;   // lexer + parser + AST
    double segundosSemantica = 0.0;
    double segundosInterpretacao = 0.0;
};

inline Compilacao compilar(const std::string& fonte) {
    Compilacao resultado;

    auto inicio = Relogio::now();
    std::string erro;
    ast::Arvore programa = parseToAst(fonte, &erro);
    resultado.segundosFrontend = segundosDesde(inicio);
    if (programa == nullptr) {
        resultado.erro = "sintaxe: " + erro;
        return resultado;
    }

    inicio = Relogio::now();
    semantica::AnalisadorSemantico analisador;
    const bool semanticaOk = analisador.analisar(*programa);
    resultado.segundosSemantica = segundosDesde(inicio);
    if (!semanticaOk) {
        resultado.erro = "semântica: " + analisador.erros().front().formatado();
        return resultado;
    }

    inicio = Relogio::now();
    Interpretador interpretador;
    const bool execucaoOk = interpretador.executar(*programa);
    resultado.segundosInterpretacao = segundosDesde(inicio);
    if (!execucaoOk) {
        resultado.erro = "execução: " + interpretador.erro();
        return resultado;
    }

    resultado.timeline = interpretador.timeline();
    resultado.ok = true;
    return resultado;
}

// O que sai do motor de áudio quando a Timeline toca inteira, renderizada fora
// do dispositivo: os mesmos blocos de `kPeriodSizeInFrames` que o callback do
// miniaudio pediria, passando por `AudioEngine::render` (síntese + limitador
// do master bus), sem placa de som e sem relógio de parede.
struct Reproducao {
    bool filaCarregada = false;
    std::uint64_t framesEsperados = 0;   // duração da Timeline, em frames
    std::uint64_t framesRenderizados = 0;
    std::uint64_t ultimoFrameComSom = 0; // 0 se nada soou
    std::size_t notasMudas = 0;          // pico abaixo de 1% do volume
    std::size_t notasAtenuadas = 0;      // pico abaixo de 50% do volume
    std::size_t blocosComUnderrun = 0;
    float pico = 0.0f;
    bool amostraInvalida = false;        // NaN ou infinito na saída
    bool filaEsvaziada = false;          // todo evento saiu da fila
    double segundosFila = 0.0;
    double segundosRender = 0.0;
};

// Folga renderizada depois do fim esperado, para pegar som que vaze além dele.
constexpr double kFolgaSegundos = 0.1;

inline Reproducao reproduzirOffline(const Timeline& timeline) {
    constexpr double kTaxa = AudioEngine::kSampleRate;
    Reproducao resultado;

    auto inicio = Relogio::now();
    // Mesma capacidade que o `main.cpp` usa: exata, porque a timeline inteira
    // entra antes de o áudio começar (SCH-05, #29).
    SpscRingBuffer<SoundEvent> fila(std::max<std::size_t>(timeline.size(), 1));
    resultado.filaCarregada = carregarFila(timeline, fila);
    resultado.segundosFila = segundosDesde(inicio);
    if (!resultado.filaCarregada) {
        return resultado;
    }

    // No heap: o AudioEngine carrega o TimelinePlayer e o vetor de vozes.
    auto audio = std::make_unique<AudioEngine>();
    audio->setRingBuffer(&fila);
    audio->setExpectedDuration(timeline.duration());

    resultado.framesEsperados =
        static_cast<std::uint64_t>(std::llround(timeline.duration() * kTaxa));
    const std::uint64_t total =
        resultado.framesEsperados + static_cast<std::uint64_t>(kFolgaSegundos * kTaxa);
    std::vector<float> saida(static_cast<std::size_t>(total), 0.0f);

    inicio = Relogio::now();
    for (std::uint64_t frame = 0; frame < total;
         frame += AudioEngine::kPeriodSizeInFrames) {
        const auto bloco = static_cast<std::uint32_t>(std::min<std::uint64_t>(
            AudioEngine::kPeriodSizeInFrames, total - frame));
        audio->render(saida.data() + frame, bloco);
        if (audio->checkAndClearUnderrun()) {
            ++resultado.blocosComUnderrun;
        }
    }
    resultado.segundosRender = segundosDesde(inicio);
    resultado.framesRenderizados = total;

    SoundEvent sobra;
    resultado.filaEsvaziada = !fila.pop(&sobra);

    for (std::uint64_t i = 0; i < total; ++i) {
        const float amostra = saida[static_cast<std::size_t>(i)];
        if (!std::isfinite(amostra)) {
            resultado.amostraInvalida = true;
            continue;
        }
        const float modulo = std::fabs(amostra);
        resultado.pico = std::max(resultado.pico, modulo);
        if (modulo > 0.0f) {
            resultado.ultimoFrameComSom = i;
        }
    }

    // Uma nota "soou" se o pico dentro da janela dela chegou perto do volume
    // pedido. A linguagem não tem notas simultâneas, então a janela de uma nota
    // não contém o som de outra.
    for (const SoundEvent& evento : timeline.events()) {
        if (evento.frequency == 0.0 || evento.volume == 0.0f) {
            continue;
        }
        const auto de = static_cast<std::uint64_t>(std::llround(evento.startTime * kTaxa));
        const auto ate = std::min<std::uint64_t>(
            total, static_cast<std::uint64_t>(std::llround(evento.endTime() * kTaxa)));
        float picoNota = 0.0f;
        for (std::uint64_t i = de; i < ate; ++i) {
            picoNota = std::max(picoNota, std::fabs(saida[static_cast<std::size_t>(i)]));
        }
        if (picoNota < 0.01f * evento.volume) {
            ++resultado.notasMudas;
        } else if (picoNota < 0.5f * evento.volume) {
            ++resultado.notasAtenuadas;
        }
    }
    return resultado;
}

inline std::size_t contarNotas(const Timeline& timeline) {
    return static_cast<std::size_t>(std::count_if(
        timeline.events().begin(), timeline.events().end(),
        [](const SoundEvent& evento) { return evento.frequency != 0.0; }));
}

}  // namespace estresse
}  // namespace mus

#endif  // MUS_TESTS_ESTRESSE_CARGAS_H
