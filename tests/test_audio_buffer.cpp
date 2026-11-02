#include "doctest.h"
#include <cmath>
#include <limits>
#include <vector>

#include "audio/audio_buffer.h"
#include "core/sound_event.h"

using namespace mus;

TEST_CASE("loadFromFile lida graciosamente com caminhos incorretos") {
    // O sistema de arquivos deve rejeitar e a função deve retornar std::nullopt.
    auto bufferOpt = AudioBuffer::loadFromFile("../samples/caminho_que_nao_existe.wav");
    
    CHECK_FALSE(bufferOpt.has_value());
}

TEST_CASE("loadFromFile decodifica WAV e aloca buffer na RAM") {
    auto bufferOpt = AudioBuffer::loadFromFile("../samples/PERC_09.wav");
    if (!bufferOpt.has_value()) {
        bufferOpt = AudioBuffer::loadFromFile("../samples/perc_09.wav");
    }
#ifdef MUS_SOURCE_DIR
    if (!bufferOpt.has_value()) {
        bufferOpt = AudioBuffer::loadFromFile(std::string(MUS_SOURCE_DIR) + "/samples/PERC_09.wav");
    }
#endif
    
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

TEST_CASE("sampleLinear: interpolação em índices inteiros e fracionários (DSP-06, #36)") {
    AudioBuffer buf;
    buf.frameCount = 4;
    buf.channels = 1;
    buf.sampleRate = 44100;
    buf.data = {0.0f, 1.0f, 0.0f, -1.0f};

    // Índices exatos
    CHECK(buf.sampleLinear(0.0) == doctest::Approx(0.0f));
    CHECK(buf.sampleLinear(1.0) == doctest::Approx(1.0f));
    CHECK(buf.sampleLinear(2.0) == doctest::Approx(0.0f));
    CHECK(buf.sampleLinear(3.0) == doctest::Approx(-1.0f));

    // Frações exatas: interpolação linear s0 + frac * (s1 - s0)
    CHECK(buf.sampleLinear(0.5) == doctest::Approx(0.5f));
    CHECK(buf.sampleLinear(0.25) == doctest::Approx(0.25f));
    CHECK(buf.sampleLinear(0.75) == doctest::Approx(0.75f));
    CHECK(buf.sampleLinear(1.5) == doctest::Approx(0.5f));
    CHECK(buf.sampleLinear(2.5) == doctest::Approx(-0.5f));

    // Suavização no final do buffer: fade out para zero sem clique
    CHECK(buf.sampleLinear(3.5) == doctest::Approx(-0.5f));
}

TEST_CASE("sampleLinear: fora dos limites e casos extremos (DSP-06, #36)") {
    AudioBuffer buf;
    buf.frameCount = 2;
    buf.channels = 1;
    buf.sampleRate = 44100;
    buf.data = {0.5f, 0.8f};

    // Fora dos limites
    CHECK(buf.sampleLinear(-0.1) == 0.0f);
    CHECK(buf.sampleLinear(-10.0) == 0.0f);
    CHECK(buf.sampleLinear(2.0) == 0.0f);
    CHECK(buf.sampleLinear(10.0) == 0.0f);

    // Buffer vazio
    AudioBuffer vazio;
    CHECK(vazio.sampleLinear(0.0) == 0.0f);
    CHECK(vazio.sampleLinear(1.5) == 0.0f);

    // Valores não finitos (NaN, Infinito)
    const double qnan = std::numeric_limits<double>::quiet_NaN();
    const double pinf = std::numeric_limits<double>::infinity();
    const double ninf = -std::numeric_limits<double>::infinity();

    CHECK(buf.sampleLinear(qnan) == 0.0f);
    CHECK(buf.sampleLinear(pinf) == 0.0f);
    CHECK(buf.sampleLinear(ninf) == 0.0f);
}

TEST_CASE("readFramesLinear: taxa de reprodução unitária, oitava acima e oitava abaixo (DSP-06, #36)") {
    AudioBuffer buf;
    buf.frameCount = 4;
    buf.channels = 1;
    buf.sampleRate = 44100;
    buf.data = {0.0f, 0.4f, 0.8f, 1.0f};

    SUBCASE("Rate 1.0 (velocidade normal)") {
        double cursor = 0.0;
        std::vector<float> out(4, 0.0f);
        std::uint32_t lidos = buf.readFramesLinear(out.data(), 4, cursor, 1.0);

        CHECK(lidos == 4);
        CHECK(out[0] == doctest::Approx(0.0f));
        CHECK(out[1] == doctest::Approx(0.4f));
        CHECK(out[2] == doctest::Approx(0.8f));
        CHECK(out[3] == doctest::Approx(1.0f));
        CHECK(cursor == doctest::Approx(4.0));
    }

    SUBCASE("Rate 2.0 (+1 oitava / velocidade dobrada)") {
        double cursor = 0.0;
        std::vector<float> out(3, 0.0f);
        // Avança de 2 em 2: lê frame 0 (0.0), frame 2 (0.8), frame 4 (fora do buffer -> 0.0)
        std::uint32_t lidos = buf.readFramesLinear(out.data(), 3, cursor, 2.0);

        CHECK(lidos == 2);
        CHECK(out[0] == doctest::Approx(0.0f));
        CHECK(out[1] == doctest::Approx(0.8f));
        CHECK(out[2] == doctest::Approx(0.0f));
        CHECK(cursor == doctest::Approx(4.0));
    }

    SUBCASE("Rate 0.5 (-1 oitava / metade da velocidade)") {
        double cursor = 0.0;
        std::vector<float> out(5, 0.0f);
        // Avança de 0.5 em 0.5: lê frame 0.0 (0.0), 0.5 (0.2), 1.0 (0.4), 1.5 (0.6), 2.0 (0.8)
        std::uint32_t lidos = buf.readFramesLinear(out.data(), 5, cursor, 0.5);

        CHECK(lidos == 5);
        CHECK(out[0] == doctest::Approx(0.0f));
        CHECK(out[1] == doctest::Approx(0.2f));
        CHECK(out[2] == doctest::Approx(0.4f));
        CHECK(out[3] == doctest::Approx(0.6f));
        CHECK(out[4] == doctest::Approx(0.8f));
        CHECK(cursor == doctest::Approx(2.5));
    }

    SUBCASE("Aplicação de volume") {
        double cursor = 0.0;
        std::vector<float> out(2, 0.0f);
        buf.readFramesLinear(out.data(), 2, cursor, 1.0, 0.5f);

        CHECK(out[0] == doctest::Approx(0.0f));
        CHECK(out[1] == doctest::Approx(0.2f)); // 0.4 * 0.5
    }

    SUBCASE("Parâmetros inválidos ou não finitos preenchem com zero") {
        const double qnan = std::numeric_limits<double>::quiet_NaN();
        const double pinf = std::numeric_limits<double>::infinity();

        double cursor = 0.0;
        std::vector<float> out(2, 999.0f);

        // Rate não finito ou não positivo
        CHECK(buf.readFramesLinear(out.data(), 2, cursor, qnan) == 0);
        CHECK(out[0] == 0.0f);
        CHECK(out[1] == 0.0f);

        std::fill(out.begin(), out.end(), 999.0f);
        CHECK(buf.readFramesLinear(out.data(), 2, cursor, -1.0) == 0);
        CHECK(out[0] == 0.0f);

        // Cursor não finito
        double cursorInvalido = pinf;
        std::fill(out.begin(), out.end(), 999.0f);
        CHECK(buf.readFramesLinear(out.data(), 2, cursorInvalido, 1.0) == 0);
        CHECK(out[0] == 0.0f);

        // Buffer vazio
        AudioBuffer vazio;
        std::fill(out.begin(), out.end(), 999.0f);
        cursor = 0.0;
        CHECK(vazio.readFramesLinear(out.data(), 2, cursor, 1.0) == 0);
        CHECK(out[0] == 0.0f);
    }
}

TEST_CASE("renderEvent: integração com SoundEvent e eventos silenciosos (DSP-06, #36)") {
    AudioBuffer buf;
    buf.frameCount = 4;
    buf.channels = 1;
    buf.sampleRate = 44100;
    buf.data = {0.2f, 0.4f, 0.6f, 0.8f};

    // Evento com Lá 4 (440 Hz) e afinação base de 440 Hz -> rate 1.0
    SoundEvent evNormal{0.0, 0.1, 440.0, 1.0f};
    double cursor1 = 0.0;
    std::vector<float> out1(4, 0.0f);
    CHECK(buf.renderEvent(evNormal, out1.data(), 4, cursor1, 440.0) == 4);
    CHECK(out1[0] == doctest::Approx(0.2f));
    CHECK(out1[1] == doctest::Approx(0.4f));

    // Evento com Lá 5 (880 Hz) e afinação base de 440 Hz -> rate 2.0 (+1 oitava)
    SoundEvent evAgudo{0.0, 0.1, 880.0, 0.5f};
    double cursor2 = 0.0;
    std::vector<float> out2(2, 0.0f);
    CHECK(buf.renderEvent(evAgudo, out2.data(), 2, cursor2, 440.0) == 2);
    // Frame 0: 0.2 * 0.5 = 0.1
    // Frame 2: 0.6 * 0.5 = 0.3
    CHECK(out2[0] == doctest::Approx(0.1f));
    CHECK(out2[1] == doctest::Approx(0.3f));
    CHECK(cursor2 == doctest::Approx(4.0));

    SUBCASE("Evento silencioso (pausa na Timeline com frequency == 0.0) não toca sample") {
        SoundEvent evSilencio{0.0, 0.5, 0.0, 1.0f};
        double cursorSilencio = 1.0;
        std::vector<float> outSilencio(3, 999.0f);

        const auto lidos = buf.renderEvent(evSilencio, outSilencio.data(), 3, cursorSilencio, 440.0);
        CHECK(lidos == 0);
        // Saída é zerada (silêncio)
        CHECK(outSilencio[0] == 0.0f);
        CHECK(outSilencio[1] == 0.0f);
        CHECK(outSilencio[2] == 0.0f);
        // Cursor não avança em silêncio
        CHECK(cursorSilencio == doctest::Approx(1.0));
    }

    SUBCASE("Evento com volume zero ou frequência negativa preenche com silêncio") {
        SoundEvent evSemVolume{0.0, 0.5, 440.0, 0.0f};
        double cursorV = 0.0;
        std::vector<float> outV(2, 1.0f);

        CHECK(buf.renderEvent(evSemVolume, outV.data(), 2, cursorV, 440.0) == 0);
        CHECK(outV[0] == 0.0f);
        CHECK(outV[1] == 0.0f);
        CHECK(cursorV == doctest::Approx(0.0));

        SoundEvent evFreqNegativa{0.0, 0.5, -440.0, 1.0f};
        CHECK(buf.renderEvent(evFreqNegativa, outV.data(), 2, cursorV, 440.0) == 0);
        CHECK(outV[0] == 0.0f);
    }
}
