/**
 * @file sample_paths.h
 * @brief Resolução portável do diretório de samples WAV em tempo de execução.
 */

#ifndef MUS_CLI_SAMPLE_PATHS_H
#define MUS_CLI_SAMPLE_PATHS_H

#include <filesystem>
#include <string>

namespace mus {

/**
 * @brief Entradas de ambiente consultadas na busca pelo diretório `samples/`.
 *
 * Isolar esses parâmetros em uma estrutura permite exercitar todas as regras de
 * precedência em testes unitários determinísticos, sem depender da existência
 * do diretório de compilação (`MUS_SOURCE_DIR`) na máquina onde o teste roda.
 */
struct ContextoBuscaSamples {
    std::string envSamplesDir;                ///< Conteúdo de `MUS_SAMPLES_DIR` (vazio se ausente).
    std::filesystem::path caminhoExecutavel;  ///< Caminho do executável resolvido no SO.
    std::filesystem::path fallbackCompilacao; ///< Caminho `<MUS_SOURCE_DIR>/samples` de desenvolvimento.
};

/**
 * @brief Descobre o caminho do próprio executável em Linux, macOS ou Windows.
 * @param argv0 Valor de `argv[0]` usado como fallback caso a consulta ao SO falhe.
 * @return Caminho do executável ou caminho vazio se não puder ser determinado.
 */
std::filesystem::path obterCaminhoExecutavel(const char* argv0);

/**
 * @brief Resolve o diretório `samples/` seguindo a ordem de precedência portável:
 *
 * 1. `MUS_SAMPLES_DIR` (quando definida e não vazia — sem fallback silencioso);
 * 2. `<dir_do_executavel>/samples` (pacote distribuído com `samples/` ao lado do binário);
 * 3. `<dir_do_executavel>/../samples` (build CMake fora da árvore, ex.: `build/compilador`);
 * 4. `fallbackCompilacao` (`MUS_SOURCE_DIR/samples`, usado em desenvolvimento);
 * 5. `<dir_do_executavel>/samples` (ou `fallbackCompilacao` / `"samples"`) como caminho diagnóstico padrão em erro.
 *
 * O diretório atual (`cwd`) não entra na busca para evitar que uma pasta `./samples`
 * de trabalho sombreie os samples do compilador. Quem precisar sobrescrever o banco
 * deve definir `MUS_SAMPLES_DIR`.
 *
 * @param ctx Contexto contendo variáveis e diretórios candidatos.
 * @return Caminho escolhido para carregamento dos arquivos `.wav`.
 */
std::filesystem::path resolverDiretorioSamples(const ContextoBuscaSamples& ctx);

/**
 * @brief Coleta o contexto real do processo atual e resolve o diretório `samples/`.
 * @param argv0 Primeiro argumento da linha de comando (`argv[0]`).
 * @param fallbackCompilacao Caminho de fallback definido na compilação (opcional).
 * @return Caminho escolhido para carregamento dos arquivos `.wav`.
 */
std::filesystem::path resolverDiretorioSamplesPadrao(
    const char* argv0,
    const std::filesystem::path& fallbackCompilacao = {});

}  // namespace mus

#endif  // MUS_CLI_SAMPLE_PATHS_H
