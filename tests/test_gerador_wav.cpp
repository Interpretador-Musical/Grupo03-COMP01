#include "doctest.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

#include "audio/audio_buffer.h"
#include "audio/sample_bank.h"
#include "codegen/gerador_wav.h"
#include "core/frames.h"
#include "core/pitch.h"
#include "core/playhead.h"
#include "core/sound_event.h"
#include "frontend/frontend.h"
#include "interpreter/interpreter.h"
#include "otimizacao/dobra_constantes.h"
#include "semantic/analisador_semantico.h"

namespace {

constexpr double kPi = 3.14159265358979323846;

struct WavDecodificado {
    std::uint32_t chunkSize = 0;
    std::uint32_t subchunk1Size = 0;
    std::uint16_t audioFormat = 0;
    std::uint16_t channels = 0;
    std::uint32_t sampleRate = 0;
    std::uint32_t byteRate = 0;
    std::uint16_t blockAlign = 0;
    std::uint16_t bitsPerSample = 0;
    std::uint32_t dataSize = 0;
    std::vector<std::int16_t> pcm16;
    std::vector<float> amostras;
};

std::uint16_t lerU16LE(const std::string& bytes, std::size_t offset) {
    const auto b0 = static_cast<std::uint8_t>(bytes[offset]);
    const auto b1 = static_cast<std::uint8_t>(bytes[offset + 1]);
    return static_cast<std::uint16_t>(static_cast<std::uint16_t>(b0) |
                                      (static_cast<std::uint16_t>(b1) << 8u));
}

std::uint32_t lerU32LE(const std::string& bytes, std::size_t offset) {
    const auto b0 = static_cast<std::uint8_t>(bytes[offset]);
    const auto b1 = static_cast<std::uint8_t>(bytes[offset + 1]);
    const auto b2 = static_cast<std::uint8_t>(bytes[offset + 2]);
    const auto b3 = static_cast<std::uint8_t>(bytes[offset + 3]);
    return static_cast<std::uint32_t>(b0) |
           (static_cast<std::uint32_t>(b1) << 8u) |
           (static_cast<std::uint32_t>(b2) << 16u) |
           (static_cast<std::uint32_t>(b3) << 24u);
}

WavDecodificado decodificarWav(const std::string& bytes) {
    REQUIRE(bytes.size() >= mus::codegen::kWavHeaderSize);
    CHECK(bytes.substr(0, 4) == "RIFF");
    CHECK(bytes.substr(8, 4) == "WAVE");
    CHECK(bytes.substr(12, 4) == "fmt ");
    CHECK(bytes.substr(36, 4) == "data");

    WavDecodificado wav;
    wav.chunkSize = lerU32LE(bytes, 4);
    wav.subchunk1Size = lerU32LE(bytes, 16);
    wav.audioFormat = lerU16LE(bytes, 20);
    wav.channels = lerU16LE(bytes, 22);
    wav.sampleRate = lerU32LE(bytes, 24);
    wav.byteRate = lerU32LE(bytes, 28);
    wav.blockAlign = lerU16LE(bytes, 32);
    wav.bitsPerSample = lerU16LE(bytes, 34);
    wav.dataSize = lerU32LE(bytes, 40);

    CHECK(wav.chunkSize == bytes.size() - 8u);
    CHECK(wav.subchunk1Size == 16u);
    CHECK(wav.audioFormat == 1u);
    CHECK(wav.channels == mus::codegen::kWavChannels);
    CHECK(wav.sampleRate == mus::codegen::kWavSampleRate);
    CHECK(wav.byteRate == 88200u);
    CHECK(wav.blockAlign == 2u);
    CHECK(wav.bitsPerSample == mus::codegen::kWavBitsPerSample);
    CHECK(wav.dataSize == bytes.size() - mus::codegen::kWavHeaderSize);
    REQUIRE(wav.dataSize % 2u == 0u);

    const std::size_t totalFrames = wav.dataSize / 2u;
    wav.pcm16.reserve(totalFrames);
    wav.amostras.reserve(totalFrames);

    for (std::size_t i = 0; i < totalFrames; ++i) {
        const std::uint16_t raw =
            lerU16LE(bytes, mus::codegen::kWavHeaderSize + i * 2u);
        const auto amostraPcm = static_cast<std::int16_t>(raw);
        wav.pcm16.push_back(amostraPcm);
        wav.amostras.push_back(static_cast<float>(amostraPcm) / 32767.0f);
    }

    return wav;
}

double magnitudeGoertzel(const std::vector<float>& amostras,
                         std::size_t inicio,
                         std::size_t quantidade,
                         double freqHz,
                         std::uint32_t sampleRate = mus::codegen::kWavSampleRate) {
    REQUIRE(inicio + quantidade <= amostras.size());
    REQUIRE(quantidade > 1);

    const double omega =
        (2.0 * kPi * freqHz) / static_cast<double>(sampleRate);
    const double coeff = 2.0 * std::cos(omega);

    double s1 = 0.0;
    double s2 = 0.0;
    for (std::size_t k = 0; k < quantidade; ++k) {
        const double janela =
            0.5 * (1.0 - std::cos((2.0 * kPi * static_cast<double>(k)) /
                                  static_cast<double>(quantidade - 1)));
        const double x = static_cast<double>(amostras[inicio + k]) * janela;
        const double s = x + coeff * s1 - s2;
        s2 = s1;
        s1 = s;
    }

    const double potencia = s1 * s1 + s2 * s2 - s1 * s2 * coeff;
    return std::sqrt(std::max(0.0, potencia)) / static_cast<double>(quantidade);
}

float picoAbsolutoJanela(const std::vector<float>& amostras,
                         std::size_t inicio,
                         std::size_t fim) {
    REQUIRE(inicio <= fim);
    REQUIRE(fim <= amostras.size());
    float pico = 0.0f;
    for (std::size_t i = inicio; i < fim; ++i) {
        pico = std::max(pico, std::fabs(amostras[i]));
    }
    return pico;
}

double rmsJanela(const std::vector<float>& amostras,
                 std::size_t inicio,
                 std::size_t fim) {
    REQUIRE(inicio < fim);
    REQUIRE(fim <= amostras.size());
    double soma = 0.0;
    for (std::size_t i = inicio; i < fim; ++i) {
        const double v = static_cast<double>(amostras[i]);
        soma += v * v;
    }
    return std::sqrt(soma / static_cast<double>(fim - inicio));
}

mus::Timeline compilarParaTimeline(const std::string& fonte,
                                   bool otimizar = true,
                                   const mus::SampleBank* sampleBank = nullptr) {
    std::string erro;
    auto ast = mus::parseToAst(fonte, &erro);
    REQUIRE_MESSAGE(ast != nullptr, "erro de sintaxe: " << erro);

    mus::semantica::AnalisadorSemantico analisador;
    REQUIRE_MESSAGE(analisador.analisar(*ast), "erro semântico inesperado");

    if (otimizar) {
        mus::otimizacao::dobrarConstantes(*ast);
    }

    mus::Interpretador interpretador;
    interpretador.setSampleBank(sampleBank);
    REQUIRE_MESSAGE(interpretador.executar(*ast),
                    "erro de execução: " << interpretador.erro());
    return interpretador.timeline();
}

std::string lerArquivoTexto(const std::string& caminhoRelativo) {
    const std::string caminho = std::string(MUS_SOURCE_DIR) + caminhoRelativo;
    std::ifstream arq(caminho, std::ios::binary);
    REQUIRE_MESSAGE(arq.good(), "não foi possível abrir: " << caminho);
    return std::string(std::istreambuf_iterator<char>(arq),
                       std::istreambuf_iterator<char>());
}

std::string lerArquivoBinario(const std::filesystem::path& caminho) {
    std::ifstream arq(caminho, std::ios::binary);
    REQUIRE_MESSAGE(arq.good(), "não foi possível abrir: " << caminho.string());
    return std::string(std::istreambuf_iterator<char>(arq),
                       std::istreambuf_iterator<char>());
}

struct ArquivoTemporario {
    std::filesystem::path caminho;

    explicit ArquivoTemporario(const std::string& nome)
        : caminho(std::filesystem::temp_directory_path() / nome) {
        std::error_code ec;
        std::filesystem::remove(caminho, ec);
    }

    ~ArquivoTemporario() {
        std::error_code ec;
        std::filesystem::remove(caminho, ec);
    }

    ArquivoTemporario(const ArquivoTemporario&) = delete;
    ArquivoTemporario& operator=(const ArquivoTemporario&) = delete;
};

void verificarEventosContraWav(const mus::Timeline& timeline,
                               const WavDecodificado& wav) {
    constexpr std::uint64_t kMargemEnvelope = 441;  // 10 ms (> 5 ms de ataque/release)
    for (const mus::SoundEvent& ev : timeline.events()) {
        const std::uint64_t ini =
            mus::segundosParaFrame(ev.startTime, mus::codegen::kWavSampleRate);
        const std::uint64_t fim =
            mus::segundosParaFrame(ev.endTime(), mus::codegen::kWavSampleRate);
        REQUIRE(fim <= wav.amostras.size());
        if (fim <= ini + 2 * kMargemEnvelope) {
            continue;
        }

        const std::size_t janIni = static_cast<std::size_t>(ini + kMargemEnvelope);
        const std::size_t janFim = static_cast<std::size_t>(fim - kMargemEnvelope);
        const std::size_t janTam = janFim - janIni;

        if (ev.type == mus::SoundEventType::Synth && ev.frequency == 0.0 &&
            ev.volume == 0.0f) {
            CHECK(picoAbsolutoJanela(wav.amostras, janIni, janFim) ==
                  doctest::Approx(0.0f));
        } else if (ev.type == mus::SoundEventType::Synth && ev.frequency > 0.0 &&
                   ev.volume > 0.0f) {
            CHECK(picoAbsolutoJanela(wav.amostras, janIni, janFim) > 0.01f);
            const double magAlvo =
                magnitudeGoertzel(wav.amostras, janIni, janTam, ev.frequency);
            const double magAbaixo =
                magnitudeGoertzel(wav.amostras, janIni, janTam, ev.frequency * 0.85);
            const double magAcima =
                magnitudeGoertzel(wav.amostras, janIni, janTam, ev.frequency * 1.15);
            CHECK(magAlvo > magAbaixo * 5.0);
            CHECK(magAlvo > magAcima * 5.0);
        }
    }
}

}  // namespace

TEST_CASE("comp-c: floatParaPcm16 converte, satura e rejeita não finitos") {
    CHECK(mus::codegen::floatParaPcm16(0.0f) == 0);
    CHECK(mus::codegen::floatParaPcm16(1.0f) == 32767);
    CHECK(mus::codegen::floatParaPcm16(-1.0f) == -32767);
    CHECK(mus::codegen::floatParaPcm16(2.5f) == 32767);
    CHECK(mus::codegen::floatParaPcm16(-3.0f) == -32767);
    CHECK(mus::codegen::floatParaPcm16(std::numeric_limits<float>::quiet_NaN()) == 0);
    CHECK(mus::codegen::floatParaPcm16(std::numeric_limits<float>::infinity()) == 0);
    CHECK(mus::codegen::floatParaPcm16(-std::numeric_limits<float>::infinity()) == 0);
}

TEST_CASE("comp-c: timeline vazia produz cabeçalho RIFF/WAVE válido de 44 bytes") {
    mus::Timeline vazia;
    std::ostringstream out(std::ios::binary);
    const mus::codegen::ResultadoWav res = mus::codegen::gerarWav(vazia, out);

    REQUIRE(res.ok);
    CHECK(res.erro.empty());
    CHECK(res.totalFrames == 0);
    CHECK(res.totalBytes == mus::codegen::kWavHeaderSize);
    CHECK(res.duracaoSegundos == doctest::Approx(0.0));

    const std::string bytes = out.str();
    REQUIRE(bytes.size() == mus::codegen::kWavHeaderSize);
    const WavDecodificado wav = decodificarWav(bytes);
    CHECK(wav.chunkSize == 36u);
    CHECK(wav.dataSize == 0u);
    CHECK(wav.amostras.empty());
}

TEST_CASE("comp-c: escala musical gera WAV com todas as alturas da IR") {
    const std::string fonte = lerArquivoTexto("/exemplos/escala.mus");
    const mus::Timeline timeline = compilarParaTimeline(fonte);

    std::ostringstream out(std::ios::binary);
    const mus::codegen::ResultadoWav res = mus::codegen::gerarWav(timeline, out);

    REQUIRE(res.ok);
    CHECK(res.totalFrames == 238140);  // 5.4 s * 44100 Hz
    CHECK(res.totalBytes == 44u + 238140u * 2u);
    CHECK(res.duracaoSegundos == doctest::Approx(5.4));

    const WavDecodificado wav = decodificarWav(out.str());
    REQUIRE(wav.amostras.size() == 238140u);
    verificarEventosContraWav(timeline, wav);
}

TEST_CASE("comp-c: programa com repetições e pausa final (arpejo.mus)") {
    const std::string fonte = lerArquivoTexto("/exemplos/arpejo.mus");
    const mus::Timeline timeline = compilarParaTimeline(fonte);
    REQUIRE(timeline.size() == 14);

    std::ostringstream out(std::ios::binary);
    const mus::codegen::ResultadoWav res = mus::codegen::gerarWav(timeline, out);

    REQUIRE(res.ok);
    CHECK(res.totalFrames == 176400);  // 4.0 s * 44100 Hz

    const WavDecodificado wav = decodificarWav(out.str());
    REQUIRE(wav.amostras.size() == 176400u);

    verificarEventosContraWav(timeline, wav);

    // Pausa final de 3.5 s a 4.0 s deve ser silêncio absoluto desde o frame exato.
    const std::size_t inicioPausa = static_cast<std::size_t>(
        mus::segundosParaFrame(3.5, mus::codegen::kWavSampleRate));
    CHECK(picoAbsolutoJanela(wav.amostras, inicioPausa, wav.amostras.size()) ==
          doctest::Approx(0.0f));
}

TEST_CASE("comp-c: duração, posicionamento temporal e mudança de andamento") {
    // 120 BPM: 0.5 s/tempo -> pausa [0.0, 0.25), do4 [0.25, 0.75), pausa [0.75, 1.0)
    // 240 BPM: 0.25 s/tempo -> la4 [1.0, 1.25)
    const mus::Timeline timeline = compilarParaTimeline(
        "andamento 120\n"
        "pausa por 0.5\n"
        "toca do4 por 1\n"
        "pausa por 0.5\n"
        "andamento 240\n"
        "toca la4 por 1\n");

    std::ostringstream out(std::ios::binary);
    const mus::codegen::ResultadoWav res = mus::codegen::gerarWav(timeline, out);
    REQUIRE(res.ok);
    CHECK(res.totalFrames ==
          mus::segundosParaFrame(1.25, mus::codegen::kWavSampleRate));

    const WavDecodificado wav = decodificarWav(out.str());
    const std::size_t f025 = static_cast<std::size_t>(
        mus::segundosParaFrame(0.25, mus::codegen::kWavSampleRate));
    const std::size_t f075 = static_cast<std::size_t>(
        mus::segundosParaFrame(0.75, mus::codegen::kWavSampleRate));
    const std::size_t f100 = static_cast<std::size_t>(
        mus::segundosParaFrame(1.00, mus::codegen::kWavSampleRate));

    CHECK(picoAbsolutoJanela(wav.amostras, 0, f025) == doctest::Approx(0.0f));
    CHECK(picoAbsolutoJanela(wav.amostras, f075, f100) == doctest::Approx(0.0f));
    CHECK(picoAbsolutoJanela(wav.amostras, f025 + 441, f075 - 441) > 0.5f);
    CHECK(picoAbsolutoJanela(wav.amostras, f100 + 441, wav.amostras.size() - 441) > 0.5f);
    verificarEventosContraWav(timeline, wav);
}

TEST_CASE("comp-c: preserva frequências (inclusive microtom) e proporção de volumes") {
    const mus::Timeline timeline = compilarParaTimeline(
        "andamento 60\n"
        "volume 0.2\n"
        "toca la4 + 0.5 por 1\n"
        "volume 0.6\n"
        "toca la4 + 0.5 por 1\n");

    std::ostringstream out(std::ios::binary);
    const mus::codegen::ResultadoWav res = mus::codegen::gerarWav(timeline, out);
    REQUIRE(res.ok);

    const WavDecodificado wav = decodificarWav(out.str());
    const double freqMicrotom = mus::midiToFrequency(69.5);

    const std::size_t ini1 = 2205;
    const std::size_t fim1 = 44100 - 2205;
    const std::size_t ini2 = 44100 + 2205;
    const std::size_t fim2 = 88200 - 2205;

    CHECK(magnitudeGoertzel(wav.amostras, ini1, fim1 - ini1, freqMicrotom) >
          magnitudeGoertzel(wav.amostras, ini1, fim1 - ini1, 440.0) * 5.0);
    CHECK(magnitudeGoertzel(wav.amostras, ini2, fim2 - ini2, freqMicrotom) >
          magnitudeGoertzel(wav.amostras, ini2, fim2 - ini2, 440.0) * 5.0);

    const double rms1 = rmsJanela(wav.amostras, ini1, fim1);
    const double rms2 = rmsJanela(wav.amostras, ini2, fim2);
    CHECK(rms1 > 0.05);
    CHECK(rms2 / rms1 == doctest::Approx(3.0).epsilon(0.02));
}

TEST_CASE("comp-c: eventos simultâneos e polirritmia no WAV final") {
    SUBCASE("acorde de Dó maior preserva as três frequências simultaneamente") {
        const mus::Timeline timeline = compilarParaTimeline(
            "andamento 60\n"
            "volume 0.25\n"
            "{ toca do4 por 1, toca mi4 por 1, toca sol4 por 1 }\n");

        std::ostringstream out(std::ios::binary);
        const mus::codegen::ResultadoWav res = mus::codegen::gerarWav(timeline, out);
        REQUIRE(res.ok);
        CHECK(res.totalFrames == 44100);

        const WavDecodificado wav = decodificarWav(out.str());
        const std::size_t ini = 2205;
        const std::size_t tam = 44100 - 4410;

        const double magDo =
            magnitudeGoertzel(wav.amostras, ini, tam, mus::midiToFrequency(60));
        const double magMi =
            magnitudeGoertzel(wav.amostras, ini, tam, mus::midiToFrequency(64));
        const double magSol =
            magnitudeGoertzel(wav.amostras, ini, tam, mus::midiToFrequency(67));
        const double magLa =
            magnitudeGoertzel(wav.amostras, ini, tam, mus::midiToFrequency(69));

        const double menorPresente = std::min({magDo, magMi, magSol});
        CHECK(menorPresente > 0.04);
        CHECK(magLa < menorPresente * 0.05);
    }

    SUBCASE("polirritmia.mus completo gera 6.0 s sem saturação indevida") {
        const std::string fonte = lerArquivoTexto("/exemplos/polirritmia.mus");
        const mus::Timeline timeline = compilarParaTimeline(fonte);

        std::ostringstream out(std::ios::binary);
        const mus::codegen::ResultadoWav res = mus::codegen::gerarWav(timeline, out);
        REQUIRE(res.ok);
        CHECK(res.totalFrames == 264600);

        const WavDecodificado wav = decodificarWav(out.str());
        CHECK(picoAbsolutoJanela(wav.amostras, 0, wav.amostras.size()) > 0.5f);
    }
}

TEST_CASE("comp-c: sintetiza samples WAV do SampleBank na saída final") {
    mus::SampleBank bank;
    const std::string caminho = std::string(MUS_SOURCE_DIR) + "/samples/PERC_09.wav";
    const auto id = bank.load("PERC_09", caminho);
    REQUIRE(id.has_value());

    const mus::Timeline timeline = compilarParaTimeline(
        "andamento 120\n"
        "toca \"PERC_09\" por 0.5\n"
        "pausa por 1\n",
        true,
        &bank);

    std::ostringstream out(std::ios::binary);
    const mus::codegen::ResultadoWav res =
        mus::codegen::gerarWav(timeline, out, &bank);
    REQUIRE(res.ok);
    CHECK(res.totalFrames ==
          mus::segundosParaFrame(0.75, mus::codegen::kWavSampleRate));

    const WavDecodificado wav = decodificarWav(out.str());
    const std::size_t fimSample = static_cast<std::size_t>(
        mus::segundosParaFrame(0.25, mus::codegen::kWavSampleRate));
    CHECK(picoAbsolutoJanela(wav.amostras, 0, fimSample) > 0.01f);
    CHECK(picoAbsolutoJanela(wav.amostras, fimSample, wav.amostras.size()) ==
          doctest::Approx(0.0f));
}

TEST_CASE("comp-c: arquivo WAV gerado pode ser consumido sem repetir parsing") {
    const mus::Timeline timeline = compilarParaTimeline(
        "andamento 120\n"
        "volume 0.5\n"
        "toca do4 por 0.5\n"
        "toca sol4 por 0.5\n");

    std::ostringstream memoria(std::ios::binary);
    const mus::codegen::ResultadoWav resMem = mus::codegen::gerarWav(timeline, memoria);
    REQUIRE(resMem.ok);
    const WavDecodificado wavMem = decodificarWav(memoria.str());

    ArquivoTemporario temp("comp_c_consumo_independente.wav");
    const mus::codegen::ResultadoWav resArq =
        mus::codegen::gerarWavArquivo(timeline, temp.caminho.string());
    REQUIRE(resArq.ok);
    CHECK(resArq.totalFrames == resMem.totalFrames);
    CHECK(resArq.totalBytes == resMem.totalBytes);
    CHECK(lerArquivoBinario(temp.caminho) == memoria.str());

    const auto decodificado = mus::AudioBuffer::loadFromFile(temp.caminho.string());
    REQUIRE(decodificado.has_value());
    CHECK(decodificado->sampleRate == mus::codegen::kWavSampleRate);
    CHECK(decodificado->channels == mus::codegen::kWavChannels);
    CHECK(decodificado->frameCount == resArq.totalFrames);
    REQUIRE(decodificado->data.size() == wavMem.amostras.size());

    float maxDiff = 0.0f;
    for (std::size_t i = 0; i < wavMem.amostras.size(); ++i) {
        maxDiff = std::max(maxDiff, std::fabs(decodificado->data[i] - wavMem.amostras[i]));
    }
    CHECK(maxDiff < 1e-4f);
}

TEST_CASE("comp-c: geração é determinística e preservada pela dobra de constantes") {
    const std::string fonte =
        "andamento 120\n"
        "volume 0.4 + 0.1\n"
        "{ toca la4 - 9 por 1 / 2, toca do4 + 4 por 1 / 2 }\n";

    const mus::Timeline tlComDobra = compilarParaTimeline(fonte, true);
    const mus::Timeline tlSemDobra = compilarParaTimeline(fonte, false);

    mus::Timeline tlDesordenada;
    tlDesordenada.add(mus::SoundEvent{0.5, 0.25, 392.0, 0.5f});
    tlDesordenada.add(mus::SoundEvent{0.0, 0.25, 261.625565, 0.5f});
    mus::Timeline tlOrdenada = tlDesordenada;
    tlOrdenada.sortByStartTime();

    std::ostringstream a(std::ios::binary);
    std::ostringstream b(std::ios::binary);
    std::ostringstream c(std::ios::binary);
    std::ostringstream d(std::ios::binary);

    REQUIRE(mus::codegen::gerarWav(tlComDobra, a).ok);
    REQUIRE(mus::codegen::gerarWav(tlSemDobra, b).ok);
    REQUIRE(mus::codegen::gerarWav(tlDesordenada, c).ok);
    REQUIRE(mus::codegen::gerarWav(tlOrdenada, d).ok);

    CHECK(a.str() == b.str());
    CHECK(c.str() == d.str());
}

TEST_CASE("comp-c: trata falhas de entrada e I/O sem gerar arquivo parcial") {
    const mus::Timeline valida = compilarParaTimeline("toca do4 por 0.5\n");

    SUBCASE("caminho vazio") {
        const mus::codegen::ResultadoWav res =
            mus::codegen::gerarWavArquivo(valida, "");
        CHECK_FALSE(res.ok);
        CHECK_FALSE(res.erro.empty());
    }

    SUBCASE("diretório pai inexistente") {
        const std::filesystem::path inexistente =
            std::filesystem::temp_directory_path() /
            "comp_c_dir_inexistente_108" / "saida.wav";
        const mus::codegen::ResultadoWav res =
            mus::codegen::gerarWavArquivo(valida, inexistente.string());
        CHECK_FALSE(res.ok);
        CHECK(res.erro.find(inexistente.string()) != std::string::npos);
        CHECK_FALSE(std::filesystem::exists(inexistente));
    }

    SUBCASE("fluxo de saída com erro") {
        std::ostringstream out;
        out.setstate(std::ios::badbit);
        const mus::codegen::ResultadoWav res = mus::codegen::gerarWav(valida, out);
        CHECK_FALSE(res.ok);
        CHECK_FALSE(res.erro.empty());
    }

    SUBCASE("evento com duração NaN não cria arquivo") {
        mus::Timeline invalida;
        invalida.add(mus::SoundEvent{0.0, std::numeric_limits<double>::quiet_NaN(), 440.0, 0.5f});

        ArquivoTemporario temp("comp_c_nan_invalido.wav");
        const mus::codegen::ResultadoWav res =
            mus::codegen::gerarWavArquivo(invalida, temp.caminho.string());
        CHECK_FALSE(res.ok);
        CHECK_FALSE(res.erro.empty());
        CHECK_FALSE(std::filesystem::exists(temp.caminho));
    }

    SUBCASE("evento com duração negativa não cria arquivo") {
        mus::Timeline invalida;
        invalida.add(mus::SoundEvent{0.0, -0.5, 440.0, 0.5f});

        ArquivoTemporario temp("comp_c_neg_invalido.wav");
        const mus::codegen::ResultadoWav res =
            mus::codegen::gerarWavArquivo(invalida, temp.caminho.string());
        CHECK_FALSE(res.ok);
        CHECK_FALSE(res.erro.empty());
        CHECK_FALSE(std::filesystem::exists(temp.caminho));
    }

    SUBCASE("duração acima do limite de 4 GiB do container WAV não cria arquivo") {
        mus::Timeline gigante;
        gigante.add(mus::SoundEvent{0.0, 60000.0, 440.0, 0.5f});

        ArquivoTemporario temp("comp_c_4gb_invalido.wav");
        const mus::codegen::ResultadoWav res =
            mus::codegen::gerarWavArquivo(gigante, temp.caminho.string());
        CHECK_FALSE(res.ok);
        CHECK(res.erro.find("4 GiB") != std::string::npos);
        CHECK_FALSE(std::filesystem::exists(temp.caminho));
    }
}
