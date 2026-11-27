// Medição dos chiados da reprodução (BUG-AUDIO, #109).
//
// Compila cada programa pelo pipeline inteiro, renderiza a mixagem fora do
// dispositivo, bloco a bloco, com o mesmo `renderRealTime` que o callback chama,
// e passa a mesma mixagem por duas cadeias de master bus:
//
// - "antes": só o softClip, como era até a #109;
// - "depois": o limitador de pico e depois o softClip, como `render()` faz agora.
//
// Imprime uma tabela em Markdown. É de onde vêm os números de `docs/chiados.md`:
//
//   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
//   cmake --build build --target mus_chiados
//   ./build/tests/mus_chiados                  # casos fixos + exemplos/
//   ./build/tests/mus_chiados meu.mus          # outros programas
//   ./build/tests/mus_chiados --wav saida/     # grava antes/depois em WAV
//
// Não faz parte do CTest: os testes de regressão estão em `test_limiter.cpp`,
// e esta tabela serve para investigar e para o relatório.
//
// A métrica principal é a energia acima de 4 kHz sobre a energia total. As
// notas dos casos ficam abaixo de 1 kHz (o staccato chega a 784 Hz), então o
// que passa do filtro é distorção ou quina na onda, e é isso que o ouvido
// escuta como chiado. Os samples de percussão têm agudo de verdade, e neles
// essa métrica não separa defeito de timbre.

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "audio/audio_engine.h"
#include "audio/limiter.h"
#include "audio/sample_bank.h"
#include "core/ring_buffer.h"
#include "core/timeline_queue.h"
#include "frontend/frontend.h"
#include "interpreter/interpreter.h"
#include "semantic/analisador_semantico.h"

using namespace mus;
namespace fs = std::filesystem;

namespace {

constexpr double kDoisPi = 6.283185307179586;

struct Caso {
    std::string nome;
    std::string fonte;
};

// 32 notas sustentadas, uma por voz, e mais oito por cima: cada nota nova
// rouba uma voz e passa pelo fade-out de 2 ms da DSP-07. Volume baixo para a
// soma não chegar ao limitador e não misturar as duas medidas.
std::string rouboDeVoz() {
    const char* notas[] = {"do", "re", "mi", "fa", "sol", "la", "si"};
    std::string fonte = "andamento 120\nvolume 0.02\n{ ";
    for (int i = 0; i < AudioEngine::kMaxVoices; ++i) {
        fonte += std::string("toca ") + notas[i % 7] + std::to_string(2 + i / 7) + " por 4, ";
    }
    fonte += "\n pausa por 1\n repita 8 vezes { toca do6 por 0.25 } }\n";
    return fonte;
}

std::vector<Caso> casosFixos() {
    return {
        {"nota no volume 1", "andamento 60\ntoca la4 por 1\n"},
        {"acorde no volume 1",
         "andamento 90\n{ toca do4 por 2, toca mi4 por 2, toca sol4 por 2 }\n"},
        {"acorde no volume 0.33",
         "andamento 90\nvolume 0.33\n"
         "{ toca do4 por 2, toca mi4 por 2, toca sol4 por 2 }\n"},
        {"acorde de 6 no volume 0.5",
         "andamento 90\nvolume 0.5\n"
         "{ toca do3 por 2, toca sol3 por 2, toca do4 por 2,"
         " toca mi4 por 2, toca sol4 por 2, toca do5 por 2 }\n"},
        {"graves no volume 1", "andamento 60\n{ toca do1 por 2, toca do2 por 2 }\n"},
        {"staccato a 1200 BPM",
         "andamento 1200\nrepita 64 vezes { toca do5 por 0.5\n toca sol5 por 0.5 }\n"},
        {"roubo de voz (33 notas)", rouboDeVoz()},
        {"bateria (samples)",
         "andamento 120\n"
         "repita 8 vezes { { toca \"PERC_01\" por 0.5, toca \"BASS_1\" por 0.5 } }\n"},
    };
}

// Os exemplos de `exemplos/` que compilam (o de erros semânticos fica de fora).
std::vector<Caso> casosDosExemplos() {
    std::vector<Caso> casos;
    for (const char* nome : {"arpejo", "escala", "polirritmia"}) {
        const fs::path caminho = fs::path(MUS_SOURCE_DIR) / "exemplos" / (std::string(nome) + ".mus");
        std::ifstream arquivo(caminho);
        std::stringstream conteudo;
        conteudo << arquivo.rdbuf();
        casos.push_back({std::string("exemplos/") + nome + ".mus", conteudo.str()});
    }
    return casos;
}

bool carregarSamples(SampleBank& banco) {
    std::vector<fs::path> arquivos;
    for (const auto& entrada : fs::directory_iterator(fs::path(MUS_SOURCE_DIR) / "samples")) {
        if (entrada.path().extension() == ".wav") {
            arquivos.push_back(entrada.path());
        }
    }
    std::sort(arquivos.begin(), arquivos.end());
    for (const fs::path& arquivo : arquivos) {
        if (!banco.load(arquivo.stem().string(), arquivo.string()).has_value()) {
            return false;
        }
    }
    return true;
}

// Energia acima de 4 kHz sobre a total, em dB: quatro biquads passa-altas RBJ
// iguais em cascata (8ª ordem). O mesmo filtro de `test_limiter.cpp`.
double energiaAcimaDe4kHzEmDb(const std::vector<float>& sinal) {
    const double w0 = kDoisPi * 4000.0 / AudioEngine::kSampleRate;
    const double alfa = std::sin(w0) / (2.0 * 0.7071067811865476);
    const double c = std::cos(w0);
    const double a0 = 1.0 + alfa;
    const double b0 = (1.0 + c) / 2.0 / a0;
    const double b1 = -(1.0 + c) / a0;
    const double a1 = -2.0 * c / a0;
    const double a2 = (1.0 - alfa) / a0;

    double estado[4][4] = {};
    double agudo = 0.0;
    double total = 0.0;
    for (float amostra : sinal) {
        double y = amostra;
        for (auto& e : estado) {
            const double saida = b0 * y + b1 * e[0] + b0 * e[1] - a1 * e[2] - a2 * e[3];
            e[1] = e[0];
            e[0] = y;
            e[3] = e[2];
            e[2] = saida;
            y = saida;
        }
        agudo += y * y;
        total += static_cast<double>(amostra) * amostra;
    }
    return (total > 0.0 && agudo > 0.0) ? 10.0 * std::log10(agudo / total) : -999.0;
}

// Amostras em que a segunda diferença passa de 0.05: quina na onda. Uma
// senoide de amplitude 1 a 1 kHz não passa de 0.02.
std::size_t contarQuinas(const std::vector<float>& sinal) {
    std::size_t quinas = 0;
    for (std::size_t i = 2; i < sinal.size(); ++i) {
        if (std::fabs(sinal[i] - 2.0f * sinal[i - 1] + sinal[i - 2]) > 0.05f) {
            ++quinas;
        }
    }
    return quinas;
}

float maiorModulo(const std::vector<float>& sinal) {
    float maior = 0.0f;
    for (float amostra : sinal) {
        maior = std::max(maior, std::fabs(amostra));
    }
    return maior;
}

void gravarWav(const fs::path& caminho, const std::vector<float>& sinal) {
    std::ofstream saida(caminho, std::ios::binary);
    const auto escrever32 = [&](std::uint32_t v) { saida.write(reinterpret_cast<const char*>(&v), 4); };
    const auto escrever16 = [&](std::uint16_t v) { saida.write(reinterpret_cast<const char*>(&v), 2); };
    const auto bytes = static_cast<std::uint32_t>(sinal.size() * 2);
    saida.write("RIFF", 4);
    escrever32(36 + bytes);
    saida.write("WAVEfmt ", 8);
    escrever32(16);
    escrever16(1);  // PCM
    escrever16(1);  // mono
    escrever32(AudioEngine::kSampleRate);
    escrever32(AudioEngine::kSampleRate * 2);
    escrever16(2);
    escrever16(16);
    saida.write("data", 4);
    escrever32(bytes);
    for (float amostra : sinal) {
        const auto valor = static_cast<std::int16_t>(
            std::lround(std::clamp(amostra, -1.0f, 1.0f) * 32767.0f));
        escrever16(static_cast<std::uint16_t>(valor));
    }
}

std::string nomeDeArquivo(const std::string& nome) {
    std::string limpo;
    for (char c : nome) {
        limpo += (std::isalnum(static_cast<unsigned char>(c)) != 0) ? c : '_';
    }
    return limpo;
}

void medir(const Caso& caso, const SampleBank& banco, const std::string& dirWav) {
    std::string erro;
    auto programa = parseToAst(caso.fonte, &erro);
    if (programa == nullptr) {
        std::printf("| %s | sintaxe: %s |\n", caso.nome.c_str(), erro.c_str());
        return;
    }
    semantica::AnalisadorSemantico analisador;
    if (!analisador.analisar(*programa)) {
        std::printf("| %s | erro semântico |\n", caso.nome.c_str());
        return;
    }
    Interpretador interpretador;
    interpretador.setSampleBank(&banco);
    if (!interpretador.executar(*programa)) {
        std::printf("| %s | %s |\n", caso.nome.c_str(), interpretador.erro().c_str());
        return;
    }

    const Timeline& timeline = interpretador.timeline();
    SpscRingBuffer<SoundEvent> fila(timeline.size());
    if (!carregarFila(timeline, fila)) {
        std::printf("| %s | fila cheia |\n", caso.nome.c_str());
        return;
    }
    AudioEngine engine;
    engine.setRingBuffer(&fila);
    engine.setSampleBank(&banco);
    engine.setExpectedDuration(timeline.duration());

    // A mixagem crua, antes do master bus, mais 100 ms para os releases.
    const auto frames = static_cast<std::size_t>(
        std::ceil((timeline.duration() + 0.1) * AudioEngine::kSampleRate));
    std::vector<float> mixagem;
    std::vector<float> bloco(AudioEngine::kPeriodSizeInFrames);
    int maisVozes = 0;
    while (mixagem.size() < frames) {
        engine.renderRealTime(bloco.data(), AudioEngine::kPeriodSizeInFrames);
        mixagem.insert(mixagem.end(), bloco.begin(), bloco.end());
        maisVozes = std::max(maisVozes, engine.activeVoiceCount());
    }

    std::size_t acimaDoLimiar = 0;
    for (float amostra : mixagem) {
        if (std::fabs(amostra) > kSoftClipThreshold) {
            ++acimaDoLimiar;
        }
    }

    std::vector<float> antes(mixagem);
    softClipBuffer(antes.data(), static_cast<std::uint32_t>(antes.size()));

    std::vector<float> depois(mixagem);
    LimitadorDePico limitador(AudioEngine::kSampleRate);
    limitador.processarBuffer(depois.data(), static_cast<std::uint32_t>(depois.size()));
    softClipBuffer(depois.data(), static_cast<std::uint32_t>(depois.size()));

    std::printf("| %s | %d | %.3f | %.1f%% | %.1f | %.1f | %zu | %zu | %.3f | %.3f |\n",
                caso.nome.c_str(), maisVozes, static_cast<double>(maiorModulo(mixagem)),
                100.0 * static_cast<double>(acimaDoLimiar) / static_cast<double>(mixagem.size()),
                energiaAcimaDe4kHzEmDb(antes), energiaAcimaDe4kHzEmDb(depois),
                contarQuinas(antes), contarQuinas(depois),
                static_cast<double>(maiorModulo(antes)), static_cast<double>(maiorModulo(depois)));

    if (!dirWav.empty()) {
        const std::string base = nomeDeArquivo(caso.nome);
        gravarWav(fs::path(dirWav) / (base + "_antes.wav"), antes);
        gravarWav(fs::path(dirWav) / (base + "_depois.wav"), depois);
    }
}

}  // namespace

int main(int argc, char** argv) {
    std::string dirWav;
    std::vector<Caso> casos;
    for (int i = 1; i < argc; ++i) {
        const std::string argumento = argv[i];
        if (argumento == "--wav") {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "--wav precisa de um diretório\n");
                return 1;
            }
            dirWav = argv[++i];
            fs::create_directories(dirWav);
            continue;
        }
        // Um diretório abre sem erro no POSIX e seria lido como vazio.
        std::ifstream arquivo(argumento);
        if (!arquivo.is_open() || fs::is_directory(argumento)) {
            std::fprintf(stderr, "não foi possível abrir '%s'\n", argumento.c_str());
            return 1;
        }
        std::stringstream conteudo;
        conteudo << arquivo.rdbuf();
        casos.push_back({argumento, conteudo.str()});
    }
    if (casos.empty()) {
        casos = casosFixos();
        for (Caso& caso : casosDosExemplos()) {
            casos.push_back(std::move(caso));
        }
    }

    SampleBank banco;
    if (!carregarSamples(banco)) {
        std::fprintf(stderr, "falha ao carregar samples/\n");
        return 1;
    }

    std::printf("| Programa | Vozes | Pico da mixagem | Acima de 0.8 |"
                " > 4 kHz antes (dB) | > 4 kHz depois (dB) |"
                " Quinas antes | Quinas depois | Pico antes | Pico depois |\n");
    std::printf("|---|--:|--:|--:|--:|--:|--:|--:|--:|--:|\n");
    for (const Caso& caso : casos) {
        medir(caso, banco, dirWav);
    }
    return 0;
}
