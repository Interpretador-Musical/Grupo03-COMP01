#ifndef MUS_INTERPRETER_INTERPRETER_H
#define MUS_INTERPRETER_INTERPRETER_H

#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "core/playhead.h"
#include "interpreter/ast.h"

namespace mus {

// Resultado de avaliar uma Expressao. Notas já resolvidas (MIDI) e literais
// numéricos compartilham o mesmo alternative `double` — a linguagem não
// distingue inteiro de real em tempo de execução, só em tempo de léxico
// (NFR da CMP-02, #9).
using Valor = std::variant<double, bool>;

// Percorre a AST mínima de `interpreter/ast.h` (tree-walking recursivo, ver
// "Crafting Interpreters" de Robert Nystrom) e produz a Timeline de
// SoundEvents que a INT-03 (#19) pede. Não toca som: isso é trabalho de quem
// consome `timeline()` depois (ver `core/timeline_player.h`) — mantém a
// lógica de linguagem separada da lógica de áudio, como o resto do projeto já
// faz.
class Interpretador {
public:
    // Dó central = MIDI 60, a mesma convenção do lexer (`scanner::analisarNota`)
    // e do `--tokens`: nota sem oitava escrita (`toca do`) usa a oitava
    // corrente do interpretador.
    static constexpr int kOitavaPadrao = 4;

    // Executa o programa inteiro. Devolve false em erro (variável não
    // definida, `andamento`/`repita` com valor inválido); a mensagem de erro
    // fica em erro(). Sem exceções — mesmo idioma de `parseString`,
    // `Playhead::setBpm` e `Options.ok`.
    bool executar(const std::vector<ast::Comando*>& programa);

    const Timeline& timeline() const { return timeline_; }
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

    Valor avaliarExpressao(const ast::Expressao& expressao);
    static double paraNumero(const Valor& valor);
    static bool paraBooleano(const Valor& valor);

    // Só grava a primeira falha: uma vez que `falhou_` é true, todo visitor
    // devolve cedo (checa `falhou_` no início) em vez de propagar lixo — não
    // há exceção para interromper a recursão, então a checagem é manual.
    void falhar(const std::string& mensagem);

    std::unordered_map<std::string, Valor> variaveis_;
    Playhead playhead_;
    Timeline timeline_;
    int oitavaCorrente_ = kOitavaPadrao;
    float volumeCorrente_ = 1.0f;
    bool falhou_ = false;
    std::string erro_;
};

}  // namespace mus

#endif  // MUS_INTERPRETER_INTERPRETER_H
