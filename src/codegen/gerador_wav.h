#ifndef MUS_CODEGEN_GERADOR_WAV_H
#define MUS_CODEGEN_GERADOR_WAV_H

#include <cstddef>
#include <cstdint>
#include <ostream>
#include <string>

#include "core/playhead.h"

namespace mus {

class SampleBank;

namespace codegen {

// Parâmetros fixos da saída-alvo musical (COMP-C, #108): WAV PCM 16-bit
// linear little-endian, mono, a 44 100 Hz (mesma taxa do AudioEngine).
constexpr std::uint32_t kWavSampleRate = 44100;
constexpr std::uint16_t kWavChannels = 1;
constexpr std::uint16_t kWavBitsPerSample = 16;
constexpr std::size_t kWavHeaderSize = 44;

// Resultado da etapa de geração da saída-alvo WAV a partir da Timeline (IR).
struct ResultadoWav {
    bool ok = false;
    std::uint64_t totalFrames = 0;
    std::uint64_t totalBytes = 0;
    double duracaoSegundos = 0.0;
    std::string erro;
};

// Converte uma amostra normalizada em ponto flutuante [-1.0, 1.0] para PCM
// linear assinado de 16 bits. Valores não finitos viram silêncio (0) e valores
// fora do intervalo são saturados para [-32768, 32767].
std::int16_t floatParaPcm16(float amostra);

// Gera um arquivo RIFF/WAVE PCM 16-bit no fluxo `destino` a partir da
// representação intermediária `timeline`, renderizando offline o mesmo motor
// de síntese (AudioEngine) usado na reprodução ao vivo, sem abrir dispositivo
// de áudio e sem repetir o parsing do código-fonte.
ResultadoWav gerarWav(const Timeline& timeline,
                      std::ostream& destino,
                      const SampleBank* sampleBank = nullptr);

// Versão em arquivo da geração WAV. Valida a timeline e o caminho antes de
// criar o arquivo e remove arquivos parciais se houver erro de escrita.
ResultadoWav gerarWavArquivo(const Timeline& timeline,
                             const std::string& caminhoArquivo,
                             const SampleBank* sampleBank = nullptr);

}  // namespace codegen
}  // namespace mus

#endif  // MUS_CODEGEN_GERADOR_WAV_H
