// Medição do teste de estresse do pipeline batch (INT-07, #43).
//
// Roda cada carga de `cargas.h` pelo pipeline inteiro e imprime uma tabela em
// Markdown com o tempo de cada fase e o que saiu do motor de áudio. É de onde
// vêm os números de `docs/estresse.md`:
//
//   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
//   cmake --build build --target mus_estresse
//   ./build/tests/mus_estresse
//
// Não faz parte do CTest de propósito: tempo de relógio depende da máquina, e
// um teste que afirma tempo falha em máquina lenta ou sob sanitizer.
//
// A reprodução é renderizada fora do dispositivo, bloco a bloco, pelo mesmo
// `AudioEngine::render` que o callback do miniaudio chama. "Fator" é quantas
// vezes mais rápido que o tempo real o motor gera o áudio: abaixo de 1, o
// callback não daria conta e haveria underrun de verdade.

#include <cstdio>
#include <string>
#include <vector>

#include "estresse/cargas.h"

using namespace mus;
using namespace mus::estresse;

namespace {

struct Linha {
    Carga carga;
    bool renderizar = true;
};

void imprimirCabecalho() {
    std::printf("| Carga | Eventos | Esperado | Gerado | Duração (s) | Menor nota (ms) |"
                " Frontend (ms) | Semântica (ms) | Interpretação (ms) | Fila (ms) |"
                " Render (ms) | Fator | Mudas | Atenuadas | Underruns | Pico |\n");
    std::printf("|---|--:|:-:|:-:|--:|--:|--:|--:|--:|--:|--:|--:|--:|--:|--:|--:|\n");
}

void medir(const Linha& linha) {
    const Carga& carga = linha.carga;
    const Compilacao compilacao = compilar(carga.fonte);
    if (!compilacao.ok) {
        std::printf("| %s | falhou: %s |\n", carga.nome.c_str(), compilacao.erro.c_str());
        return;
    }

    const Timeline& timeline = compilacao.timeline;
    const bool bate = timeline.size() == carga.eventos &&
                      contarNotas(timeline) == carga.notas;

    std::printf("| %s | %zu | %zu | %s | %.3f | %.2f | %.2f | %.2f | %.2f |",
                carga.nome.c_str(), timeline.size(), carga.eventos,
                bate ? "igual" : "**DIFERENTE**", timeline.duration(),
                carga.menorNotaSegundos * 1000.0,
                compilacao.segundosFrontend * 1000.0,
                compilacao.segundosSemantica * 1000.0,
                compilacao.segundosInterpretacao * 1000.0);

    if (!linha.renderizar) {
        std::printf(" — | — | — | — | — | — | — |\n");
        return;
    }

    const Reproducao reproducao = reproduzirOffline(timeline);
    const double fator = (reproducao.segundosRender > 0.0)
                             ? timeline.duration() / reproducao.segundosRender
                             : 0.0;
    std::printf(" %.2f | %.2f | %.0fx | %zu | %zu | %zu | %.3f%s |\n",
                reproducao.segundosFila * 1000.0, reproducao.segundosRender * 1000.0,
                fator, reproducao.notasMudas, reproducao.notasAtenuadas,
                reproducao.blocosComUnderrun, static_cast<double>(reproducao.pico),
                reproducao.amostraInvalida ? " (NaN)" : "");
}

}  // namespace

int main() {
    std::vector<Linha> linhas;

    // Andamento: mesma forma, mesma duração, BPM crescente. As duas últimas
    // passam do envelope aceito de propósito, para mostrar onde ele acaba.
    for (const double bpm : {60.0, 120.0, 240.0, 600.0, 1000.0, 1500.0, 2400.0, 6000.0}) {
        linhas.push_back({cargaGrade(bpm, 0.25, 10.0)});
    }

    // Densidade sustentada no limite aceito.
    linhas.push_back({cargaGrade(1500.0, 0.25, 60.0)});

    // Todas as construções da linguagem.
    linhas.push_back({cargaMista(100)});

    // Volume: só compilação, sem render (o áudio seria longo demais).
    linhas.push_back({cargaAninhada(600.0, 100, 1000, 0.25), false});
    linhas.push_back({cargaAninhada(600.0, 1000, 1000, 0.25), false});
    linhas.push_back({cargaAninhada(600.0, 10000, 1000, 0.25), false});
    linhas.push_back({cargaPlana(600.0, 10000, 0.25), false});
    linhas.push_back({cargaPlana(600.0, 100000, 0.25), false});

    imprimirCabecalho();
    for (const Linha& linha : linhas) {
        medir(linha);
        std::fflush(stdout);
    }
    return 0;
}
