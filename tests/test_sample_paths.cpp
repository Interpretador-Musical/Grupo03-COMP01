#include "doctest.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <system_error>

#include "cli/sample_paths.h"

namespace {

namespace fs = std::filesystem;

struct DiretorioTemporario {
    fs::path raiz;

    DiretorioTemporario() {
        const auto agora =
            std::chrono::steady_clock::now().time_since_epoch().count();
        raiz = fs::temp_directory_path() /
               ("mus_test_sample_paths_" + std::to_string(agora));
        fs::create_directories(raiz);
    }

    ~DiretorioTemporario() {
        std::error_code erro;
        fs::remove_all(raiz, erro);
    }
};

struct GuardaDiretorioAtual {
    fs::path anterior;
    bool ativo = false;

    explicit GuardaDiretorioAtual(const fs::path& novo) {
        std::error_code erro;
        anterior = fs::current_path(erro);
        if (!erro) {
            fs::current_path(novo, erro);
            ativo = !erro;
        }
    }

    ~GuardaDiretorioAtual() {
        if (ativo && !anterior.empty()) {
            std::error_code erro;
            fs::current_path(anterior, erro);
        }
    }
};

fs::path canonico(const fs::path& caminho) {
    std::error_code erro;
    const fs::path res = fs::weakly_canonical(caminho, erro);
    return erro ? caminho.lexically_normal() : res;
}

}  // namespace

TEST_CASE("INT-08: MUS_SAMPLES_DIR tem precedencia maxima e nao faz fallback silencioso") {
    DiretorioTemporario temp;
    const fs::path samplesPacote = temp.raiz / "pacote" / "samples";
    const fs::path samplesBuild = temp.raiz / "repo" / "samples";
    fs::create_directories(samplesPacote);
    fs::create_directories(samplesBuild);

    mus::ContextoBuscaSamples ctx;
    ctx.envSamplesDir = (temp.raiz / "custom_inexistente").string();
    ctx.caminhoExecutavel = temp.raiz / "pacote" / "compilador";
    ctx.fallbackCompilacao = samplesBuild;

    CHECK(mus::resolverDiretorioSamples(ctx) == fs::path(ctx.envSamplesDir));
}

TEST_CASE("INT-08: samples ao lado do executavel vence ../samples e MUS_SOURCE_DIR") {
    DiretorioTemporario temp;
    const fs::path samplesPacote = temp.raiz / "pacote" / "samples";
    const fs::path samplesFallback = temp.raiz / "repo" / "samples";
    fs::create_directories(samplesPacote);
    fs::create_directories(samplesFallback);

    mus::ContextoBuscaSamples ctx;
    ctx.caminhoExecutavel = temp.raiz / "pacote" / "compilador";
    ctx.fallbackCompilacao = samplesFallback;

    CHECK(mus::resolverDiretorioSamples(ctx) == canonico(samplesPacote));
}

TEST_CASE("INT-08: ./samples no diretorio atual nao sombreia ../samples nem MUS_SOURCE_DIR") {
    DiretorioTemporario temp;
    const fs::path workspace = temp.raiz / "workspace";
    const fs::path samplesCwd = workspace / "samples";
    const fs::path samplesPai = temp.raiz / "instalacao" / "samples";
    const fs::path samplesFallback = temp.raiz / "repo" / "samples";
    fs::create_directories(samplesCwd);
    fs::create_directories(samplesPai);
    fs::create_directories(samplesFallback);
    fs::create_directories(temp.raiz / "instalacao" / "bin");

    GuardaDiretorioAtual guarda(workspace);
    REQUIRE(guarda.ativo);

    mus::ContextoBuscaSamples ctx;
    ctx.caminhoExecutavel = temp.raiz / "instalacao" / "bin" / "compilador";
    ctx.fallbackCompilacao = samplesFallback;

    CHECK(mus::resolverDiretorioSamples(ctx) == canonico(samplesPai));

    ctx.caminhoExecutavel = temp.raiz / "sem_samples_pai" / "compilador";
    fs::create_directories(temp.raiz / "sem_samples_pai");
    CHECK(mus::resolverDiretorioSamples(ctx) == canonico(samplesFallback));

    if (const char* env = std::getenv("MUS_SAMPLES_DIR");
        env == nullptr || env[0] == '\0') {
        CHECK(mus::resolverDiretorioSamplesPadrao("mus_tests", samplesFallback) ==
              canonico(samplesFallback));
    }
}

TEST_CASE("INT-08: ../samples relativo ao binario atende build fora da arvore") {
    DiretorioTemporario temp;
    const fs::path samplesPai = temp.raiz / "projeto" / "samples";
    const fs::path samplesFallback = temp.raiz / "outro_repo" / "samples";
    fs::create_directories(samplesPai);
    fs::create_directories(samplesFallback);
    fs::create_directories(temp.raiz / "projeto" / "build");

    mus::ContextoBuscaSamples ctx;
    ctx.caminhoExecutavel = temp.raiz / "projeto" / "build" / "compilador";
    ctx.fallbackCompilacao = samplesFallback;

    CHECK(mus::resolverDiretorioSamples(ctx) == canonico(samplesPai));
}

TEST_CASE("INT-08: fallback de compilacao entra apenas quando caminhos portaveis nao existem") {
    DiretorioTemporario temp;
    const fs::path samplesFallback = temp.raiz / "repo_original" / "samples";
    fs::create_directories(samplesFallback);
    fs::create_directories(temp.raiz / "repo_original" / "build" / "Release");

    mus::ContextoBuscaSamples ctx;
    ctx.caminhoExecutavel =
        temp.raiz / "repo_original" / "build" / "Release" / "compilador";
    ctx.fallbackCompilacao = samplesFallback;

    CHECK(mus::resolverDiretorioSamples(ctx) == canonico(samplesFallback));
}

TEST_CASE("INT-08: binario isolado em outra maquina reporta caminho local ao lado do executavel") {
    DiretorioTemporario temp;
    fs::create_directories(temp.raiz / "outra_maquina");

    mus::ContextoBuscaSamples ctx;
    ctx.caminhoExecutavel = temp.raiz / "outra_maquina" / "compilador";
    ctx.fallbackCompilacao = temp.raiz / "runner_ci_inexistente" / "samples";

    CHECK(mus::resolverDiretorioSamples(ctx) ==
          canonico(temp.raiz / "outra_maquina" / "samples"));
}

TEST_CASE("INT-08: obterCaminhoExecutavel localiza o binario em execucao") {
    const fs::path caminho = mus::obterCaminhoExecutavel("mus_tests");
    CHECK_FALSE(caminho.empty());

    std::error_code erro;
    CHECK(fs::is_regular_file(caminho, erro));
    CHECK_FALSE(erro);
}
