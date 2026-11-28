#include "cli/sample_paths.h"

#include <cstdint>
#include <cstdlib>
#include <system_error>
#include <vector>

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#elif defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace mus {

namespace {

bool ehDiretorioValido(const std::filesystem::path& caminho) {
    if (caminho.empty()) {
        return false;
    }
    std::error_code erro;
    return std::filesystem::is_directory(caminho, erro) && !erro;
}

std::filesystem::path normalizarSePossivel(const std::filesystem::path& caminho) {
    std::error_code erro;
    const std::filesystem::path canonico = std::filesystem::weakly_canonical(caminho, erro);
    if (!erro && !canonico.empty()) {
        return canonico;
    }
    return caminho.lexically_normal();
}

}  // namespace

std::filesystem::path obterCaminhoExecutavel(const char* argv0) {
    namespace fs = std::filesystem;
    std::error_code erro;

#if defined(__linux__)
    const fs::path linkProc = fs::read_symlink("/proc/self/exe", erro);
    if (!erro && !linkProc.empty()) {
        return normalizarSePossivel(linkProc);
    }
#elif defined(__APPLE__)
    std::uint32_t tamanho = 0;
    static_cast<void>(_NSGetExecutablePath(nullptr, &tamanho));
    if (tamanho > 0) {
        std::vector<char> buffer(tamanho, '\0');
        if (_NSGetExecutablePath(buffer.data(), &tamanho) == 0) {
            return normalizarSePossivel(fs::path(buffer.data()));
        }
    }
#elif defined(_WIN32)
    std::vector<wchar_t> buffer(MAX_PATH, L'\0');
    for (;;) {
        const DWORD lidos = GetModuleFileNameW(
            nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (lidos == 0) {
            break;
        }
        if (lidos < buffer.size()) {
            return normalizarSePossivel(fs::path(buffer.data()));
        }
        if (buffer.size() >= 32768) {
            break;
        }
        buffer.assign(buffer.size() * 2, L'\0');
    }
#endif

    if (argv0 != nullptr && argv0[0] != '\0') {
        const fs::path bruto(argv0);
        erro.clear();
        if (bruto.has_parent_path() || fs::exists(bruto, erro)) {
            erro.clear();
            const fs::path absoluto = fs::absolute(bruto, erro);
            if (!erro && !absoluto.empty()) {
                return normalizarSePossivel(absoluto);
            }
            return normalizarSePossivel(bruto);
        }
    }

    return {};
}

std::filesystem::path resolverDiretorioSamples(const ContextoBuscaSamples& ctx) {
    namespace fs = std::filesystem;

    // 1. Sobrescrita explícita por variável de ambiente: não faz fallback
    // silencioso para não mascarar configuração inválida do usuário.
    if (!ctx.envSamplesDir.empty()) {
        return fs::path(ctx.envSamplesDir);
    }

    const fs::path dirExec = ctx.caminhoExecutavel.has_parent_path()
                                 ? ctx.caminhoExecutavel.parent_path()
                                 : fs::path{};

    // 2. Pacote portável: `samples/` distribuído ao lado do executável.
    if (!dirExec.empty()) {
        const fs::path aoLadoDoBinario = dirExec / "samples";
        if (ehDiretorioValido(aoLadoDoBinario)) {
            return normalizarSePossivel(aoLadoDoBinario);
        }
    }

    // 3. Layout padrão de build fora da árvore (`<raiz>/build/compilador`).
    if (!dirExec.empty()) {
        const fs::path paiDoBinario = dirExec / ".." / "samples";
        if (ehDiretorioValido(paiDoBinario)) {
            return normalizarSePossivel(paiDoBinario);
        }
    }

    // 4. Fallback de desenvolvimento gravado na compilação (`MUS_SOURCE_DIR/samples`).
    if (ehDiretorioValido(ctx.fallbackCompilacao)) {
        return normalizarSePossivel(ctx.fallbackCompilacao);
    }

    // 5. Nenhum candidato existe: reporta o caminho esperado ao lado do binário
    // (ou fallback), sem depender de `./samples` no diretório de trabalho.
    if (!dirExec.empty()) {
        return normalizarSePossivel(dirExec / "samples");
    }
    if (!ctx.fallbackCompilacao.empty()) {
        return ctx.fallbackCompilacao;
    }
    return fs::path("samples");
}

std::filesystem::path resolverDiretorioSamplesPadrao(
    const char* argv0,
    const std::filesystem::path& fallbackCompilacao) {
    ContextoBuscaSamples ctx;
    if (const char* env = std::getenv("MUS_SAMPLES_DIR"); env != nullptr) {
        ctx.envSamplesDir = env;
    }

    ctx.caminhoExecutavel = obterCaminhoExecutavel(argv0);
    ctx.fallbackCompilacao = fallbackCompilacao;
    return resolverDiretorioSamples(ctx);
}

}  // namespace mus
