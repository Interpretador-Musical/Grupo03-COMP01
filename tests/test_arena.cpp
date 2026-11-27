#include "doctest.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

#include "frontend/frontend.h"
#include "interpreter/arena.h"
#include "interpreter/ast.h"
#include "interpreter/interpreter.h"

using mus::ast::Arena;

// Arena da AST — MAT-10 (#55). Esta parte testa o alocador sozinho; a parte
// de baixo, que a AST inteira mora nele.

namespace {

bool alinhado(const void* endereco, std::size_t alinhamento) {
    return reinterpret_cast<std::uintptr_t>(endereco) % alinhamento == 0;
}

// O maior alinhamento que a arena aceita. Varia por plataforma: 16 no x86-64
// Linux, 8 no ARM do macOS.
struct alignas(alignof(std::max_align_t)) AlinhamentoMaximo {
    unsigned char bytes[alignof(std::max_align_t)];
};

// Conta e ordena as destruições: cada objeto anota o próprio número no vetor
// quando morre.
struct Anotador {
    Anotador(std::vector<int>* registro, int numero) : registro_(registro), numero_(numero) {}
    ~Anotador() { registro_->push_back(numero_); }
    Anotador(const Anotador&) = delete;
    Anotador& operator=(const Anotador&) = delete;

    std::vector<int>* registro_;
    int numero_;
};

}  // namespace

TEST_CASE("arena: cada objeto sai alinhado para o próprio tipo") {
    Arena arena;
    // Alternar tamanhos e alinhamentos força o cursor a se ajustar entre um e
    // outro, em vez de ficar sempre num múltiplo de 8 por sorte.
    for (int i = 0; i < 100; ++i) {
        CHECK(alinhado(arena.criar<char>('x'), alignof(char)));
        CHECK(alinhado(arena.criar<std::int32_t>(i), alignof(std::int32_t)));
        CHECK(alinhado(arena.criar<double>(1.5), alignof(double)));
        CHECK(alinhado(arena.criar<AlinhamentoMaximo>(), alignof(std::max_align_t)));
    }
}

TEST_CASE("arena: o primeiro bloco já existe antes da primeira alocação") {
    Arena arena;
    CHECK(arena.quantidadeDeBlocos() == 1);
    CHECK(arena.bytesReservados() == Arena::kPrimeiroBloco);
    CHECK(arena.bytesUsados() == 0);
}

TEST_CASE("arena: blocos novos não mexem no que já foi entregue") {
    Arena arena(1024);
    std::vector<std::int64_t*> valores;
    for (std::int64_t i = 0; i < 10000; ++i) {
        valores.push_back(arena.criar<std::int64_t>(i * 7));
    }
    CHECK(arena.quantidadeDeBlocos() > 1);
    CHECK(arena.bytesUsados() == 10000 * sizeof(std::int64_t));
    CHECK(arena.bytesReservados() >= arena.bytesUsados());
    for (std::int64_t i = 0; i < 10000; ++i) {
        REQUIRE(*valores[static_cast<std::size_t>(i)] == i * 7);
        CHECK(arena.contem(valores[static_cast<std::size_t>(i)]));
    }
}

TEST_CASE("arena: o tamanho dos blocos dobra até o teto") {
    Arena arena(1024);
    // Cada pedido é maior que o que sobrou do bloco anterior e cabe no bloco
    // seguinte, então abre exatamente um bloco novo.
    for (std::size_t tamanho : {1000, 2000, 4000, 8000}) {
        arena.alocar(tamanho, 1);
    }
    CHECK(arena.quantidadeDeBlocos() == 4);
    CHECK(arena.bytesReservados() == 1024 + 2048 + 4096 + 8192);
}

TEST_CASE("arena: pedido maior que o bloco ganha um bloco do próprio tamanho") {
    Arena arena(1024);
    auto* grande = static_cast<unsigned char*>(arena.alocar(100000, 1));
    std::memset(grande, 0xAB, 100000);
    CHECK(arena.contem(grande));
    CHECK(arena.contem(grande + 99999));
    CHECK(arena.quantidadeDeBlocos() == 2);

    // O pedido gigante não infla o bloco seguinte: ele continua sendo o dobro
    // do primeiro.
    arena.alocar(1500, 1);
    CHECK(arena.bytesReservados() == 1024 + 100000 + 2048);
}

TEST_CASE("arena: endereço de fora não é dela") {
    Arena arena;
    int local = 0;
    CHECK_FALSE(arena.contem(&local));
    CHECK_FALSE(arena.contem(nullptr));
    CHECK(arena.contem(arena.criar<int>(1)));
}

TEST_CASE("arena: tipo trivial não entra no registro de destrutores") {
    Arena arena;
    arena.criar<int>(1);
    arena.criar<double>(2.0);
    arena.copiarTexto("abc");
    CHECK(arena.objetosComDestrutor() == 0);
}

TEST_CASE("arena: destrutores rodam uma vez, na ordem inversa da criação") {
    std::vector<int> mortos;
    {
        Arena arena;
        for (int i = 0; i < 50; ++i) {
            arena.criar<Anotador>(&mortos, i);
        }
        CHECK(arena.objetosComDestrutor() == 50);
        CHECK(mortos.empty());
    }
    REQUIRE(mortos.size() == 50);
    for (int i = 0; i < 50; ++i) {
        CHECK(mortos[static_cast<std::size_t>(i)] == 49 - i);
    }
}

TEST_CASE("arena: um std::vector dentro dela é destruído e não vaza") {
    // O vetor guarda os elementos no heap comum, fora da arena. Quem prova que
    // esse buffer volta é o LeakSanitizer do job de ASan da CI.
    Arena arena;
    auto* lista = arena.criar<std::vector<int>>();
    for (int i = 0; i < 1000; ++i) {
        lista->push_back(i);
    }
    CHECK(arena.objetosComDestrutor() == 1);
    CHECK(lista->size() == 1000);
}

TEST_CASE("arena: copiarTexto devolve uma cópia que mora na arena") {
    Arena arena;
    std::string original = "PERC_09 com espaço e acento: ç";
    const std::string_view copia = arena.copiarTexto(original);
    original.assign(original.size(), '#');  // a cópia não depende da origem

    CHECK(copia == "PERC_09 com espaço e acento: ç");
    CHECK(arena.contem(copia.data()));
    CHECK(arena.copiarTexto("").empty());
}

TEST_CASE("arena: reiniciar() volta o cursor ao começo do primeiro bloco") {
    std::vector<int> mortos;
    Arena arena(1024);
    void* primeiro = arena.alocar(8, 8);
    for (int i = 0; i < 1000; ++i) {
        arena.criar<double>(1.0);
    }
    arena.criar<Anotador>(&mortos, 7);
    REQUIRE(arena.quantidadeDeBlocos() > 1);

    arena.reiniciar();

    CHECK(mortos == std::vector<int>{7});
    CHECK(arena.objetosComDestrutor() == 0);
    CHECK(arena.bytesUsados() == 0);
    CHECK(arena.quantidadeDeBlocos() == 1);
    CHECK(arena.bytesReservados() == 1024);
    // É isto que a issue pede: destruir tudo é só resetar o ponteiro, e a
    // próxima alocação cai no mesmo lugar da primeira.
    CHECK(arena.alocar(8, 8) == primeiro);

    // E a arena continua utilizável, com o crescimento recomeçando do dobro
    // do primeiro bloco.
    arena.alocar(1500, 1);
    CHECK(arena.bytesReservados() == 1024 + 2048);
}

#if defined(MUS_ARENA_ASAN)
TEST_CASE("arena: com ASan, memória não entregue ou já devolvida fica envenenada") {
    Arena arena;
    auto* valor = arena.criar<std::int64_t>(42);
    CHECK_FALSE(__asan_address_is_poisoned(valor));
    // O resto do bloco ainda não foi entregue a ninguém.
    CHECK(__asan_address_is_poisoned(valor + 1));

    arena.reiniciar();
    // O ponteiro antigo não vale mais: lê-lo agora seria acusado pelo ASan.
    CHECK(__asan_address_is_poisoned(valor));
}
#endif

// ---------------------------------------------------------------------------
// A AST inteira mora na arena (os dois critérios e o NFR da MAT-10).

namespace {

namespace ast = mus::ast;

// Percorre a árvore conferindo que cada nó, cada lista e cada texto está
// dentro da arena, e conta as listas, que são os únicos objetos com
// destrutor.
struct Varredura {
    const Arena& arena;
    int nos = 0;
    int listas = 0;
    int fora = 0;

    void confere(const void* endereco) {
        if (!arena.contem(endereco)) {
            ++fora;
        }
    }

    void texto(std::string_view valor) {
        if (!valor.empty()) {
            confere(valor.data());
        }
    }

    void expressao(const ast::Expressao* e) {
        if (e == nullptr) {
            return;
        }
        ++nos;
        confere(e);
        if (const auto* s = std::get_if<ast::ExpressaoLiteralString>(e)) {
            texto(s->valor);
        } else if (const auto* i = std::get_if<ast::ExpressaoIdentificador>(e)) {
            texto(i->nome);
        } else if (const auto* u = std::get_if<ast::ExpressaoUnaria>(e)) {
            expressao(u->operando);
        } else if (const auto* b = std::get_if<ast::ExpressaoBinaria>(e)) {
            expressao(b->esquerda);
            expressao(b->direita);
        }
    }

    void lista(const std::vector<ast::Comando*>* comandos) {
        ++listas;
        confere(comandos);
        for (const ast::Comando* c : *comandos) {
            comando(c);
        }
    }

    void comando(const ast::Comando* c) {
        ++nos;
        confere(c);
        if (const auto* a = std::get_if<ast::ComandoAtribuicao>(c)) {
            texto(a->nome);
            expressao(a->valor);
        } else if (const auto* t = std::get_if<ast::ComandoToca>(c)) {
            expressao(t->altura);
            expressao(t->duracao);
        } else if (const auto* p = std::get_if<ast::ComandoPausa>(c)) {
            expressao(p->duracao);
        } else if (const auto* an = std::get_if<ast::ComandoAndamento>(c)) {
            expressao(an->valor);
        } else if (const auto* o = std::get_if<ast::ComandoOitava>(c)) {
            expressao(o->valor);
        } else if (const auto* v = std::get_if<ast::ComandoVolume>(c)) {
            expressao(v->valor);
        } else if (const auto* f = std::get_if<ast::ComandoFiltro>(c)) {
            expressao(f->valor);
        } else if (const auto* r = std::get_if<ast::ComandoRepita>(c)) {
            expressao(r->vezes);
            lista(r->corpo);
        } else if (const auto* g = std::get_if<ast::ComandoGrupo>(c)) {
            ++listas;  // o vetor de trilhas
            confere(g->trilhas);
            for (const auto* trilha : *g->trilhas) {
                lista(trilha);
            }
        }
    }
};

// Todo tipo de nó que a gramática monta hoje.
const char* const kTodosOsNos =
    "andamento 90 + 30\n"
    "oitava 4\n"
    "volume 0.8\n"
    "filtro 600 + 200\n"
    "um_nome_comprido_o_bastante_para_nao_caber_no_sso = 0.5\n"
    "ligado = verdadeiro e nao falso\n"
    "toca \"PERC_09\" por 1\n"
    "repita 2 vezes {\n"
    "    toca do4 + -2 * (1 - 1) por um_nome_comprido_o_bastante_para_nao_caber_no_sso\n"
    "    pausa por 0.25\n"
    "    { toca mi por 1, toca sol4 por 1, }\n"
    "}\n";

}  // namespace

TEST_CASE("arena da AST: todo nó, lista e texto de uma árvore parseada está na arena") {
    std::string erro;
    ast::Arvore programa = mus::parseToAst(kTodosOsNos, &erro);
    REQUIRE_MESSAGE(programa != nullptr, erro);

    Varredura varredura{programa.arena()};
    varredura.lista(programa.get());

    CHECK(varredura.nos > 30);
    CHECK(varredura.fora == 0);
}

TEST_CASE("arena da AST: só as listas têm destrutor, nenhum nó") {
    std::string erro;
    ast::Arvore programa = mus::parseToAst(kTodosOsNos, &erro);
    REQUIRE_MESSAGE(programa != nullptr, erro);

    Varredura varredura{programa.arena()};
    varredura.lista(programa.get());
    // Raiz, corpo do `repita`, vetor de trilhas e três trilhas (a última
    // vazia).
    CHECK(varredura.listas == 6);
    CHECK(programa.arena().objetosComDestrutor() == static_cast<std::size_t>(varredura.listas));
}

TEST_CASE("arena da AST: destruir 20 mil linhas não visita nenhum nó") {
    // O NFR da issue, provado sem medir tempo: a árvore tem 100 mil nós e uma
    // lista só. Destruí-la roda um destrutor (o da raiz) e devolve os blocos.
    std::string fonte = "t = 0.25\n";
    for (int i = 0; i < 20000; ++i) {
        fonte += "toca do4 + " + std::to_string(i % 12) + " por t\n";
    }
    ast::Arvore programa = mus::parseToAst(fonte);
    REQUIRE(programa != nullptr);

    Varredura varredura{programa.arena()};
    varredura.lista(programa.get());
    CHECK(varredura.nos == 2 + 20000 * 5);
    CHECK(varredura.fora == 0);
    CHECK(programa.arena().objetosComDestrutor() == 1);
    // Uns 100 mil nós cabem em poucos blocos, no lugar de 100 mil `new`.
    CHECK(programa.arena().quantidadeDeBlocos() < 20);

    programa.reset();
    CHECK(programa == nullptr);
}

TEST_CASE("arena da AST: mover a árvore leva a arena junto") {
    ast::Arvore origem = mus::parseToAst("x = 2\ntoca do4 por x");
    REQUIRE(origem != nullptr);
    const std::vector<ast::Comando*>* raiz = origem.get();

    ast::Arvore destino = std::move(origem);
    CHECK(origem == nullptr);
    CHECK_FALSE(origem);
    REQUIRE(destino != nullptr);
    CHECK(destino.get() == raiz);
    CHECK(destino.arena().contem(raiz));

    // Os nós continuam válidos no dono novo.
    mus::Interpretador interpretador;
    CHECK(interpretador.executar(*destino));
    CHECK(interpretador.timeline().size() == 1);

    ast::Arvore outra;
    CHECK(outra == nullptr);
    outra = std::move(destino);
    CHECK(destino == nullptr);
    CHECK(nullptr != outra);
}

TEST_CASE("arena da AST: erro de sintaxe devolve árvore vazia") {
    std::string erro;
    CHECK(mus::parseToAst("repita 2 vezes { toca do4 por", &erro) == nullptr);
    CHECK_FALSE(erro.empty());
}

TEST_CASE("arena da AST: Arvore::nova monta uma árvore à mão") {
    ast::Arvore programa = ast::Arvore::nova();
    REQUIRE(programa != nullptr);
    CHECK(programa->empty());

    ast::Arena& arena = programa.arena();
    ast::Expressao* nota = arena.criar<ast::Expressao>(ast::ExpressaoNota{0, 4});
    ast::Expressao* duracao = arena.criar<ast::Expressao>(ast::ExpressaoLiteralNumero{1.0});
    programa->push_back(arena.criar<ast::Comando>(ast::ComandoToca{nota, duracao}));

    mus::Interpretador interpretador;
    CHECK(interpretador.executar(*programa));
    CHECK(interpretador.timeline().size() == 1);
}
