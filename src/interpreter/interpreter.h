/**
 * @file interpreter.h
 * @brief Interpretador tree-walking que traduz a AST na IR `Timeline`.
 */

#ifndef MUS_INTERPRETER_INTERPRETER_H
#define MUS_INTERPRETER_INTERPRETER_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "core/playhead.h"
#include "interpreter/ambiente.h"
#include "interpreter/ast.h"
#include "audio/sample_bank.h"

namespace mus {

/**
 * @brief Executa a AST validada e produz a `Timeline` de `SoundEvent`s (INT-03, #19).
 *
 * Implementa interpretação *tree-walking* recursiva mantendo separação estrita
 * entre a lógica da linguagem e a síntese sonora: nenhum áudio é tocado aqui;
 * apenas eventos temporizados são acumulados na representação intermediária
 * consumida posteriormente pelo motor de áudio ou pelo gerador WAV.
 */
class Interpretador {
public:
    /** @brief Oitava inicial padrão (`4`, onde `do4` = MIDI 60). */
    static constexpr int kOitavaPadrao = 4;

    /**
     * @name Tetos de Segurança por Programa (#124 e #125)
     * Recusam programas com crescimento desproporcional de eventos ou iterações
     * vazias antes de esgotar memória ou travar a compilação.
     *
     * Medido em Release (Apple M4, 19/11/2026): cada evento custa cerca de 100
     * bytes até a Timeline sair ordenada, então 5 milhões ficam em torno de
     * 500 MB, 5 vezes a maior carga aceita no estresse (10^6, INT-07 #43), ou
     * 35 horas de música a 600 BPM. Uma volta vazia de `repita` custa cerca de
     * 5 ns, então 10 milhões de voltas levam uns 50 ms.
     * @{
     */
    /** @brief Número máximo de eventos permitidos na `Timeline` (~500 MB no pior caso). */
    static constexpr std::size_t kMaximoEventos = 5'000'000;
    /** @brief Soma máxima de iterações de todos os laços `repita` no programa. */
    static constexpr std::uint64_t kMaximoVoltas = 10'000'000;
    /** @} */

    /**
     * @brief Configuração dos limites de eventos e voltas usados durante `executar()`.
     *
     * `voltas` soma as voltas de todos os `repita` do programa, aninhados
     * inclusive: `repita 1000 vezes { repita 1000 vezes { } }` são 1 001 000.
     */
    struct LimitesExecucao {
        std::size_t eventos = kMaximoEventos;  ///< Teto máximo de eventos na Timeline.
        std::uint64_t voltas = kMaximoVoltas;  ///< Teto máximo acumulado de voltas `repita`.
    };

    /**
     * @brief Sobrescreve os limites de execução (útil para testes de fronteira).
     * @param limites Novos tetos de eventos e voltas.
     */
    void setLimites(LimitesExecucao limites) { limites_ = limites; }

    /**
     * @brief Associa o banco de samples WAV carregado antes da interpretação (INT-05).
     *
     * O chamador deve manter `*bank` vivo durante toda a execução do interpretador.
     *
     * @param bank Ponteiro para o `SampleBank` preenchido (pode ser `nullptr`).
     */
    void setSampleBank(const SampleBank* bank) {
        sampleBank_ = bank;
    }

    /**
     * @brief Percorre a AST gerando a `Timeline` ordenada por `startTime`.
     *
     * Sem exceções: interrompe no primeiro erro de execução (variável indefinida,
     * valores fora de faixa em `andamento`/`repita`/`oitava`/`volume` ou estouro
     * de limites) e registra a mensagem em `erro()`.
     *
     * @param programa Lista raiz de comandos produzida por `parseToAst()`.
     * @return `true` em sucesso; `false` se ocorreu erro de execução.
     */
    bool executar(const std::vector<ast::Comando*>& programa);

    /**
     * @brief Registra uma função musical (closure) capturando o escopo léxico atual.
     * @param nome Nome da função para invocação posterior.
     * @param parametros Nomes posicionais dos parâmetros aceitos pela função.
     * @param corpoAst Bloco de comandos da AST associado à função.
     */
    void declararClosure(const std::string& nome,
                         std::vector<std::string> parametros,
                         const std::vector<ast::Comando*>* corpoAst);

    /**
     * @brief Invoca uma closure previamente declarada com passagem posicional de argumentos.
     * @param nome Nome da função registrada via `declararClosure()`.
     * @param argumentos Lista de valores correspondentes aos parâmetros formais.
     * @return `true` se executada sem falhas; `false` em aridade inválida, nome
     *         inexistente ou erro durante o corpo (detalhes em `erro()`).
     */
    bool invocarClosure(const std::string& nome,
                        const std::vector<Valor>& argumentos = {});

    /**
     * @brief Acessa o ambiente de variáveis do interpretador (leitura e escrita).
     * @return Referência para o `Ambiente`.
     */
    Ambiente& ambiente() { return ambiente_; }

    /**
     * @brief Acessa o ambiente de variáveis do interpretador (somente leitura).
     * @return Referência constante para o `Ambiente`.
     */
    const Ambiente& ambiente() const { return ambiente_; }

    /**
     * @brief Retorna a `Timeline` (IR) gerada pela execução.
     * @return Referência constante para a `Timeline`.
     */
    const Timeline& timeline() const { return timeline_; }

    /**
     * @brief Retorna a mensagem do primeiro erro de execução ocorrido, se houver.
     * @return Referência constante para a string de erro (vazia se sucesso).
     */
    const std::string& erro() const { return erro_; }

private:
    void executarBloco(const std::vector<ast::Comando*>& bloco);
    void executarComando(const ast::Comando& comando);
    void executarAtribuicao(const ast::ComandoAtribuicao& atribuicao);
    void executarToca(const ast::ComandoToca& toca);
    void executarPausa(const ast::ComandoPausa& pausa);
    void executarAndamento(const ast::ComandoAndamento& andamento);
    void executarOitava(const ast::ComandoOitava& oitava);
    void executarVolume(const ast::ComandoVolume& volume);
    void executarRepita(const ast::ComandoRepita& repita);
    void executarGrupo(const ast::ComandoGrupo& grupo);

    // Único caminho para um evento entrar na Timeline: recusa o programa, sem
    // adicionar nada, quando a Timeline já está no teto de eventos.
    void registrarEvento(const SoundEvent& evento);

    Valor avaliarExpressao(const ast::Expressao& expressao);
    static double paraNumero(const Valor& valor);
    static bool paraBooleano(const Valor& valor);

    // Só grava a primeira falha: uma vez que `falhou_` é true, todo visitor
    // devolve cedo (checa `falhou_` no início) em vez de propagar lixo — não
    // há exceção para interromper a recursão, então a checagem é manual.
    void falhar(const std::string& mensagem);

    const SampleBank* sampleBank_ = nullptr;
    Ambiente ambiente_;
    std::unordered_map<std::string, Closure> closures_;
    Playhead playhead_;
    Timeline timeline_;
    int oitavaCorrente_ = kOitavaPadrao;
    float volumeCorrente_ = 1.0f;
    LimitesExecucao limites_;
    std::uint64_t voltasExecutadas_ = 0;
    bool falhou_ = false;
    std::string erro_;
};

}  // namespace mus

#endif  // MUS_INTERPRETER_INTERPRETER_H
