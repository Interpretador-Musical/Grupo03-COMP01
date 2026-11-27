#include "codegen/gerador_wav.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

#include "audio/audio_engine.h"
#include "audio/sample_bank.h"
#include "core/frames.h"
#include "core/ring_buffer.h"
#include "core/sound_event.h"
#include "core/timeline_queue.h"

namespace mus {
namespace codegen {
namespace {

static_assert(kWavSampleRate == AudioEngine::kSampleRate,
              "taxa de amostragem do gerador WAV deve coincidir com a do AudioEngine");

constexpr std::uint32_t kBytesPorAmostra =
    static_cast<std::uint32_t>(kWavBitsPerSample / 8u);
constexpr std::uint32_t kBlockAlign =
    static_cast<std::uint32_t>(kWavChannels) * kBytesPorAmostra;
constexpr std::uint32_t kByteRate = kWavSampleRate * kBlockAlign;
constexpr std::uint32_t kBlockFrames = 4096;

// O container RIFF/WAVE padrão usa tamanhos de 32 bits sem sinal:
// ChunkSize = 36 + dataBytes <= 0xFFFFFFFF.
constexpr std::uint64_t kMaxWavFrames = (0xFFFFFFFFull - 36ull) / kBlockAlign;

void escreverU16LE(char* destino, std::uint16_t valor) {
    destino[0] = static_cast<char>(valor & 0xFFu);
    destino[1] = static_cast<char>((valor >> 8u) & 0xFFu);
}

void escreverU32LE(char* destino, std::uint32_t valor) {
    destino[0] = static_cast<char>(valor & 0xFFu);
    destino[1] = static_cast<char>((valor >> 8u) & 0xFFu);
    destino[2] = static_cast<char>((valor >> 16u) & 0xFFu);
    destino[3] = static_cast<char>((valor >> 24u) & 0xFFu);
}

bool validarDuracaoEFrames(const Timeline& timeline,
                           double* duracaoSaida,
                           std::uint64_t* totalFramesSaida,
                           std::string* erroSaida) {
    for (const SoundEvent& evento : timeline.events()) {
        if (!std::isfinite(evento.startTime) || !std::isfinite(evento.duration) ||
            !std::isfinite(evento.endTime())) {
            *erroSaida = "timeline contém evento com tempo ou duração não finita";
            return false;
        }
        if (evento.startTime < 0.0 || evento.duration < 0.0) {
            *erroSaida = "timeline contém evento com tempo ou duração negativa";
            return false;
        }
    }

    const double duracao = timeline.duration();
    if (!std::isfinite(duracao) || duracao < 0.0) {
        *erroSaida = "duração total da timeline é inválida para geração WAV";
        return false;
    }

    const double framesEstimados = duracao * static_cast<double>(kWavSampleRate);
    if (!std::isfinite(framesEstimados) ||
        framesEstimados > static_cast<double>(kMaxWavFrames)) {
        *erroSaida = "duração excede o limite máximo de 4 GiB do formato WAV";
        return false;
    }

    const std::uint64_t totalFrames = segundosParaFrame(duracao, kWavSampleRate);
    if (totalFrames > kMaxWavFrames) {
        *erroSaida = "duração excede o limite máximo de 4 GiB do formato WAV";
        return false;
    }

    *duracaoSaida = duracao;
    *totalFramesSaida = totalFrames;
    return true;
}

std::array<char, kWavHeaderSize> montarCabecalhoWav(std::uint32_t dataBytes) {
    std::array<char, kWavHeaderSize> header{};
    std::memcpy(header.data(), "RIFF", 4);
    escreverU32LE(header.data() + 4, 36u + dataBytes);
    std::memcpy(header.data() + 8, "WAVE", 4);
    std::memcpy(header.data() + 12, "fmt ", 4);
    escreverU32LE(header.data() + 16, 16u);
    escreverU16LE(header.data() + 20, 1u);
    escreverU16LE(header.data() + 22, kWavChannels);
    escreverU32LE(header.data() + 24, kWavSampleRate);
    escreverU32LE(header.data() + 28, kByteRate);
    escreverU16LE(header.data() + 32, static_cast<std::uint16_t>(kBlockAlign));
    escreverU16LE(header.data() + 34, kWavBitsPerSample);
    std::memcpy(header.data() + 36, "data", 4);
    escreverU32LE(header.data() + 40, dataBytes);
    return header;
}

}  // namespace

std::int16_t floatParaPcm16(float amostra) {
    if (!std::isfinite(amostra)) {
        return 0;
    }
    const float limitada = std::clamp(amostra, -1.0f, 1.0f);
    const long long escalada = std::llround(static_cast<double>(limitada) * 32767.0);
    const long long saturada = std::clamp(escalada, -32768LL, 32767LL);
    return static_cast<std::int16_t>(saturada);
}

ResultadoWav gerarWav(const Timeline& timeline,
                      std::ostream& destino,
                      const SampleBank* sampleBank,
                      float masterVolume,
                      float lpfCutoffHz) {
    ResultadoWav resultado;

    if (!destino.good()) {
        resultado.erro = "fluxo de saída WAV está em estado de erro";
        return resultado;
    }

    double duracao = 0.0;
    std::uint64_t totalFrames = 0;
    if (!validarDuracaoEFrames(timeline, &duracao, &totalFrames, &resultado.erro)) {
        return resultado;
    }

    const std::uint32_t dataBytes =
        static_cast<std::uint32_t>(totalFrames * kBlockAlign);
    const std::array<char, kWavHeaderSize> header = montarCabecalhoWav(dataBytes);
    destino.write(header.data(), static_cast<std::streamsize>(header.size()));
    if (!destino.good()) {
        resultado.erro = "falha ao escrever o cabeçalho WAV";
        return resultado;
    }

    if (totalFrames > 0) {
        SpscRingBuffer<SoundEvent> fila(timeline.size());
        if (!carregarFila(timeline, fila)) {
            resultado.erro = "falha ao carregar eventos da timeline para geração WAV";
            return resultado;
        }

        AudioEngine engine;
        engine.setRingBuffer(&fila);
        engine.setSampleBank(sampleBank);
        engine.setExpectedDuration(duracao);
        engine.setMasterVolume(masterVolume);
        engine.setLowPassCutoff(lpfCutoffHz);

        std::vector<float> bloco(kBlockFrames, 0.0f);
        std::vector<char> pcmBytes(static_cast<std::size_t>(kBlockFrames) * kBlockAlign);

        std::uint64_t framesGerados = 0;
        while (framesGerados < totalFrames) {
            const std::uint64_t restantes = totalFrames - framesGerados;
            const std::uint32_t framesNesteBloco = static_cast<std::uint32_t>(
                std::min<std::uint64_t>(restantes, kBlockFrames));

            engine.render(bloco.data(), framesNesteBloco);

            for (std::uint32_t i = 0; i < framesNesteBloco; ++i) {
                const std::int16_t pcm = floatParaPcm16(bloco[i]);
                escreverU16LE(pcmBytes.data() + static_cast<std::size_t>(i) * kBlockAlign,
                              static_cast<std::uint16_t>(pcm));
            }

            const std::size_t bytesNesteBloco =
                static_cast<std::size_t>(framesNesteBloco) * kBlockAlign;
            destino.write(pcmBytes.data(),
                          static_cast<std::streamsize>(bytesNesteBloco));
            if (!destino.good()) {
                resultado.erro = "falha ao escrever amostras PCM no fluxo WAV";
                return resultado;
            }

            framesGerados += framesNesteBloco;
        }
    }

    resultado.ok = true;
    resultado.totalFrames = totalFrames;
    resultado.totalBytes = static_cast<std::uint64_t>(kWavHeaderSize) + dataBytes;
    resultado.duracaoSegundos =
        static_cast<double>(totalFrames) / static_cast<double>(kWavSampleRate);
    return resultado;
}

ResultadoWav gerarWavArquivo(const Timeline& timeline,
                             const std::string& caminhoArquivo,
                             const SampleBank* sampleBank,
                             float masterVolume,
                             float lpfCutoffHz) {
    ResultadoWav resultado;

    if (caminhoArquivo.empty()) {
        resultado.erro = "caminho do arquivo de saída WAV não pode ser vazio";
        return resultado;
    }

    double duracao = 0.0;
    std::uint64_t totalFrames = 0;
    if (!validarDuracaoEFrames(timeline, &duracao, &totalFrames, &resultado.erro)) {
        return resultado;
    }

    std::ofstream arquivo(caminhoArquivo, std::ios::binary | std::ios::trunc);
    if (!arquivo.is_open()) {
        resultado.erro =
            "não foi possível abrir o arquivo de saída WAV: '" + caminhoArquivo + "'";
        return resultado;
    }

    resultado = gerarWav(timeline, arquivo, sampleBank, masterVolume, lpfCutoffHz);
    arquivo.flush();
    const bool arquivoOk = arquivo.good();
    arquivo.close();

    if (!resultado.ok || !arquivoOk) {
        if (resultado.ok) {
            resultado.ok = false;
            resultado.erro =
                "falha ao gravar completamente o arquivo WAV: '" + caminhoArquivo + "'";
        }
        resultado.totalFrames = 0;
        resultado.totalBytes = 0;
        resultado.duracaoSegundos = 0.0;
        std::error_code ec;
        std::filesystem::remove(caminhoArquivo, ec);
    }

    return resultado;
}

}  // namespace codegen
}  // namespace mus
