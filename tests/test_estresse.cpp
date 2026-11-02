#include "doctest.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "audio/audio_engine.h"
#include "core/ring_buffer.h"
#include "core/sound_event.h"
#include "core/timeline_queue.h"
#include "estresse/cargas.h"

// Teste de estresse do pipeline batch (INT-07, #43).
//
// Cada caso compila um programa `.mus` gerado com contagem e duração
// conhecidas (ver `estresse/cargas.h`), confere a Timeline contra a fórmula e
// depois a toca inteira pelo motor de áudio, fora do dispositivo. Os casos
// cobrem só o envelope aceito: andamento até 1500 BPM e nota de pelo menos
// 10 ms. O que fica fora dele está medido em `docs/estresse.md` e tem issue.
//
// Nenhum caso afirma tempo decorrido. As medições de tempo ficam com o
// `mus_estresse`, que não roda no CI.

using namespace mus;
using namespace mus::estresse;

namespace {

// A duração sai de somas sucessivas de `double` no Playhead, então não bate
// bit a bit com a fórmula fechada. Um bilionésimo relativo é muito menos que
// um frame em qualquer duração que estes testes geram.
constexpr double kToleranciaRelativa = 1e-9;

void conferirTimeline(const Carga& carga, const Timeline& timeline) {
    CHECK(timeline.size() == carga.eventos);
    CHECK(contarNotas(timeline) == carga.notas);
    CHECK(timeline.duration() ==
          doctest::Approx(carga.duracaoSegundos).epsilon(kToleranciaRelativa));

    // A ordem que a fila exige: o consumidor só olha o evento da frente.
    bool ordenada = true;
    for (std::size_t i = 1; i < timeline.size(); ++i) {
        if (timeline.events()[i].startTime < timeline.events()[i - 1].startTime) {
            ordenada = false;
            break;
        }
    }
    CHECK(ordenada);
}

void conferirReproducao(const Reproducao& reproducao) {
    REQUIRE(reproducao.filaCarregada);
    CHECK(reproducao.filaEsvaziada);
    CHECK_FALSE(reproducao.amostraInvalida);
    CHECK(reproducao.pico <= 1.0f);
    CHECK(reproducao.pico > 0.0f);
    CHECK(reproducao.notasMudas == 0);
    CHECK(reproducao.notasAtenuadas == 0);
    // Underrun aqui seria falso positivo: a fila inteira já está carregada.
    CHECK(reproducao.blocosComUnderrun == 0);
    // Som depois do fim esperado é a música tocando mais longa do que foi
    // compilada.
    CHECK(reproducao.ultimoFrameComSom <= reproducao.framesEsperados);
}

void compilarEReproduzir(const Carga& carga) {
    INFO("carga: ", carga.nome);
    REQUIRE(carga.menorNotaSegundos >= kMenorNotaAceitaSegundos);

    const Compilacao compilacao = compilar(carga.fonte);
    INFO("erro: ", compilacao.erro);
    REQUIRE(compilacao.ok);
    conferirTimeline(carga, compilacao.timeline);

    conferirReproducao(reproduzirOffline(compilacao.timeline));
}

}  // namespace

TEST_SUITE("estresse") {

TEST_CASE("estresse: andamentos de 60 a 1500 BPM tocam todas as notas") {
    // Três figuras por andamento: a densidade vai de 0.33 a 100 eventos por
    // segundo. 1500 BPM com figura 0.25 é a nota de 10 ms, o limite aceito.
    const double andamentos[] = {60.0, 120.0, 240.0, 600.0, 1000.0, 1500.0};
    const double figuras[] = {1.0, 0.5, 0.25};

    for (const double bpm : andamentos) {
        for (const double figura : figuras) {
            compilarEReproduzir(cargaGrade(bpm, figura, 3.0));
        }
    }
}

TEST_CASE("estresse: densidade máxima aceita sustentada por 15 segundos") {
    // 100 eventos por segundo, 1 500 eventos, todos na nota mais curta aceita.
    compilarEReproduzir(cargaGrade(1500.0, 0.25, 15.0));
}

TEST_CASE("estresse: programa com todas as construções da linguagem") {
    compilarEReproduzir(cargaMista(20));
}

TEST_CASE("estresse: um milhão de eventos compilam e entram inteiros na fila") {
    // Sem renderizar: seriam quase 7 horas de áudio. O que se verifica é a
    // geração e a entrega à fila, que é onde um volume grande quebraria.
    const Carga carga = cargaAninhada(600.0, 1000, 1000, 0.25);
    INFO("carga: ", carga.nome);

    const Compilacao compilacao = compilar(carga.fonte);
    REQUIRE(compilacao.ok);
    conferirTimeline(carga, compilacao.timeline);

    // O último início é a soma de 999 999 figuras iguais. Comparado com a conta
    // fechada, é o erro acumulado do Playhead no pior caso desta suíte.
    const double ultimoInicioExato = 999999.0 * 0.25 * 60.0 / 600.0;
    CHECK(compilacao.timeline.events().back().startTime ==
          doctest::Approx(ultimoInicioExato).epsilon(kToleranciaRelativa));

    SpscRingBuffer<SoundEvent> fila(compilacao.timeline.size());
    REQUIRE(carregarFila(compilacao.timeline, fila));

    std::size_t retirados = 0;
    double inicioAnterior = -1.0;
    bool emOrdem = true;
    SoundEvent evento;
    while (fila.pop(&evento)) {
        emOrdem = emOrdem && evento.startTime >= inicioAnterior;
        inicioAnterior = evento.startTime;
        ++retirados;
    }
    CHECK(retirados == carga.eventos);
    CHECK(emOrdem);
}

TEST_CASE("estresse: arquivo de 20 mil linhas passa pelo lexer e pelo parser") {
    // A lista de comandos do `parser.y` é recursiva à esquerda; se virasse à
    // direita, a pilha do Bison cresceria com o número de linhas e um arquivo
    // assim estouraria o YYMAXDEPTH.
    const Carga carga = cargaPlana(600.0, 20000, 0.25);
    INFO("carga: ", carga.nome);

    const Compilacao compilacao = compilar(carga.fonte);
    INFO("erro: ", compilacao.erro);
    REQUIRE(compilacao.ok);
    conferirTimeline(carga, compilacao.timeline);
}

TEST_CASE("estresse: 64 eventos sobrepostos na fila, além das 16 vozes") {
    // A sobreposição é montada direto na fila, sem passar pela linguagem, para
    // controlar exatamente quantos eventos soam juntos. Acima de 16 vozes o
    // motor rouba a mais fraca; o que se exige é que isso não estrague a saída
    // nem trave o consumo.
    //
    // Os inícios vão em ordem, como a fila exige (o motor só olha o evento da
    // frente; é o que `carregarFila` garante no caminho normal): oito grupos de
    // oito notas simultâneas, a 0, 1, ..., 7 ms. Como cada nota dura 0.2 s, as
    // 64 continuam soando juntas.
    constexpr std::size_t kSobrepostos = 64;
    SpscRingBuffer<SoundEvent> fila(kSobrepostos);
    for (std::size_t i = 0; i < kSobrepostos; ++i) {
        const double inicio = 0.001 * static_cast<double>(i / 8);
        REQUIRE(fila.push(SoundEvent{inicio, 0.2, 220.0 + 10.0 * i, 1.0f}));
    }

    AudioEngine audio;
    audio.setRingBuffer(&fila);
    audio.setExpectedDuration(0.2 + 0.007);

    std::vector<float> saida(AudioEngine::kSampleRate / 2, 0.0f);
    for (std::size_t frame = 0; frame < saida.size();
         frame += AudioEngine::kPeriodSizeInFrames) {
        const auto bloco = static_cast<std::uint32_t>(
            std::min<std::size_t>(AudioEngine::kPeriodSizeInFrames, saida.size() - frame));
        audio.render(saida.data() + frame, bloco);
    }

    SoundEvent sobra;
    CHECK_FALSE(fila.pop(&sobra));

    bool limpa = true;
    for (const float amostra : saida) {
        limpa = limpa && std::isfinite(amostra) && std::fabs(amostra) <= 1.0f;
    }
    CHECK(limpa);
    // Depois de 0.2 s + o release, todas as vozes precisam ter voltado ao
    // silêncio: voz roubada não pode ficar presa tocando.
    CHECK(saida.back() == 0.0f);
}

}  // TEST_SUITE("estresse")
