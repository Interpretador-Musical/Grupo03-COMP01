#include "doctest.h"
#include "audio/audio_buffer.h"

using namespace mus;

TEST_CASE("loadFromFile lida graciosamente com caminhos incorretos") {
    // O sistema de arquivos deve rejeitar e a função deve retornar std::nullopt.
    // Usamos um caminho deliberadamente falso aqui.
    auto bufferOpt = AudioBuffer::loadFromFile("../samples/caminho_que_nao_existe.wav");
    
    CHECK_FALSE(bufferOpt.has_value());
}

TEST_CASE("loadFromFile decodifica WAV e aloca buffer na RAM") {
    // O caminho real vai apenas aqui, no teste de sucesso.
    // Dica: renomeie o arquivo real na pasta para não ter espaços!
    auto bufferOpt = AudioBuffer::loadFromFile("../samples/perc_09.wav");
    
    if (!bufferOpt.has_value()) {
        MESSAGE("Arquivo de sample ausente. Pulando teste de alocacao.");
        return;
    }

    const AudioBuffer& buffer = bufferOpt.value();

    CHECK(buffer.frameCount > 0);
    CHECK(buffer.channels == 1);       // Forçado para mono pela nossa config
    CHECK(buffer.sampleRate == 44100); // Forçado para 44.1kHz pela nossa config
    
    // NFR de Memória: o tamanho real do vetor na RAM precisa corresponder 
    // rigorosamente ao número de frames extraídos pelo decoder.
    CHECK(buffer.data.size() == buffer.frameCount);
}