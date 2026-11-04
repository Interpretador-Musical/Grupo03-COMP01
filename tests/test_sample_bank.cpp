
#include "doctest.h"

#include <cstdint>
#include <limits>
#include <string>

#include "audio/sample_bank.h"

using mus::SampleBank;

namespace {

std::string caminhoSample(const std::string& arquivo) {
    return std::string(MUS_SOURCE_DIR) + "/samples/" + arquivo;
}

}  // namespace

TEST_CASE("INT-05: SampleBank inicia vazio") {
    SampleBank banco;

    CHECK(banco.empty());
    CHECK(banco.size() == 0);
    CHECK_FALSE(banco.findId("kick").has_value());
    CHECK(banco.get(0) == nullptr);
}

TEST_CASE("INT-05: SampleBank carrega WAV e consulta por nome e ID") {
    SampleBank banco;

    const auto id = banco.load("kick", caminhoSample("PERC_09.wav"));

    REQUIRE(id.has_value());
    CHECK(*id == 0);
    CHECK_FALSE(banco.empty());
    CHECK(banco.size() == 1);

    const auto encontrado = banco.findId("kick");
    REQUIRE(encontrado.has_value());
    CHECK(*encontrado == *id);

    const auto* buffer = banco.get(*id);
    REQUIRE(buffer != nullptr);

    CHECK(buffer->frameCount > 0);
    CHECK(buffer->channels == 1);
    CHECK(buffer->sampleRate == 44100);
    CHECK(buffer->data.size() == buffer->frameCount);
}

TEST_CASE("INT-05: SampleBank rejeita nome duplicado") {
    SampleBank banco;

    const auto original =
        banco.load("kick", caminhoSample("PERC_09.wav"));

    REQUIRE(original.has_value());

    const auto duplicado =
        banco.load("kick", caminhoSample("PERC_10.wav"));

    CHECK_FALSE(duplicado.has_value());
    CHECK(banco.size() == 1);
    CHECK(banco.findId("kick") == original);
    CHECK(banco.get(*original) != nullptr);
}

TEST_CASE("INT-05: SampleBank rejeita entradas invalidas") {
    SampleBank banco;

    CHECK_FALSE(
        banco.load("", caminhoSample("PERC_09.wav")).has_value());

    CHECK_FALSE(
        banco.load("kick", "").has_value());

    CHECK_FALSE(
        banco.load("inexistente",
                   caminhoSample("arquivo_inexistente.wav")).has_value());

    CHECK(banco.empty());
    CHECK_FALSE(banco.findId("inexistente").has_value());
}

TEST_CASE("INT-05: SampleBank utiliza IDs distintos e valida limites") {
    SampleBank banco;

    const auto kick =
        banco.load("kick", caminhoSample("PERC_09.wav"));

    const auto snare =
        banco.load("snare", caminhoSample("PERC_10.wav"));

    REQUIRE(kick.has_value());
    REQUIRE(snare.has_value());

    CHECK(*kick == 0);
    CHECK(*snare == 1);
    CHECK(*kick != *snare);
    CHECK(banco.size() == 2);

    CHECK(banco.get(*kick) != nullptr);
    CHECK(banco.get(*snare) != nullptr);

    CHECK(banco.get(2) == nullptr);
    CHECK(banco.get(std::numeric_limits<std::uint32_t>::max())
          == nullptr);
}
