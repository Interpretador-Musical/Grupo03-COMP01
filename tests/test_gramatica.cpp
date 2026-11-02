#include "doctest.h"

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "frontend/frontend.h"

// A gramática EBNF de `docs/gramatica_ebnf.md` (DOC-A, #106) confrontada com
// o `parser/parser.y`.
//
// O `parser.y` escreve as expressões de forma ambígua e desambigua com
// `%left`/`%right`/`%nonassoc`; o documento escreve a mesma linguagem sem
// ambiguidade, um não-terminal por nível de precedência. As duas só são a
// mesma linguagem se ninguém errou a conta — em especial no `nao`, que tem
// precedência menor que as comparações e por isso "engole" tudo à direita
// até o próximo `e`/`ou` (ver a seção sobre o `nao` no documento). Este
// arquivo é o que sustenta a frase "a EBNF descreve exatamente o compilador":
//
//   1. `Reconhecedor` transcreve a EBNF do documento, regra por regra;
//   2. todas as cadeias de tokens até um tamanho são passadas aos dois, e as
//      respostas precisam bater uma a uma;
//   3. os exemplos ```mus e ```mus-invalido do próprio documento são
//      compilados, para que o texto não envelheça calado.

namespace {

using mus::TipoToken;

// ---------------------------------------------------------------------------
// Transcrição da EBNF
// ---------------------------------------------------------------------------

// Cada não-terminal recebe a posição onde começa e devolve TODAS as posições
// onde pode terminar. É a leitura da EBNF como definição de conjunto, sem
// nenhuma das escolhas de desempate do Bison — se as duas aceitam as mesmas
// cadeias, é porque a EBNF está certa, e não porque o reconhecedor imitou o
// parser.
//
// O conjunto de posições é um bitmask (o bit i ligado = "pode terminar antes
// do token i"), o que limita a entrada a 63 tokens — folgado para os
// programas deste arquivo, e é o que deixa a varredura exaustiva rápida. A
// memoização por (regra, posição) evita refazer o mesmo trecho.
class Reconhecedor {
public:
    static constexpr std::size_t kMaximoTokens = 63;

    explicit Reconhecedor(std::vector<TipoToken> tokens)
        : tokens_(std::move(tokens)),
          memo_(kQuantidadeRegras * (tokens_.size() + 1)),
          calculado_(memo_.size(), false) {}

    // programa = { comando } ;
    bool aceitaPrograma() {
        return (repeticao(um(0), Regra::Comando) & um(tokens_.size())) != 0;
    }

private:
    using Posicoes = std::uint64_t;

    enum class Regra {
        Comando, Bloco, Expressao, Conjuncao, Relacional, Aditiva,
        AditivaFechada, AditivaAberta, TermoFechado, TermoAberto,
        UnarioFechado, UnarioAberto, Primario
    };
    static constexpr std::size_t kQuantidadeRegras = 13;

    std::vector<TipoToken> tokens_;
    std::vector<Posicoes> memo_;
    std::vector<bool> calculado_;

    static Posicoes um(std::size_t i) { return Posicoes{1} << i; }

    bool eh(std::size_t i, TipoToken tipo) const {
        return i < tokens_.size() && tokens_[i] == tipo;
    }

    bool ehUm(std::size_t i, std::initializer_list<TipoToken> tipos) const {
        for (TipoToken tipo : tipos) {
            if (eh(i, tipo)) {
                return true;
            }
        }
        return false;
    }

    Posicoes chama(Regra regra, std::size_t i) {
        const std::size_t chave = static_cast<std::size_t>(regra) * (tokens_.size() + 1) + i;
        if (!calculado_[chave]) {
            memo_[chave] = expande(regra, i);
            calculado_[chave] = true;
        }
        return memo_[chave];
    }

    Posicoes expande(Regra regra, std::size_t i) {
        switch (regra) {
            case Regra::Comando: return comando(i);
            case Regra::Bloco: return bloco(i);
            case Regra::Expressao: return expressao(i);
            case Regra::Conjuncao: return conjuncao(i);
            case Regra::Relacional: return relacional(i);
            case Regra::Aditiva: return aditiva(i);
            case Regra::AditivaFechada: return aditivaFechada(i);
            case Regra::AditivaAberta: return aditivaAberta(i);
            case Regra::TermoFechado: return termoFechado(i);
            case Regra::TermoAberto: return termoAberto(i);
            case Regra::UnarioFechado: return unarioFechado(i);
            case Regra::UnarioAberto: return unarioAberto(i);
            case Regra::Primario: return primario(i);
        }
        return 0;
    }

    // A seguido de B: para cada fim de A, todos os fins de B a partir dali.
    Posicoes seguido(Posicoes inicios, Regra regra) {
        Posicoes fins = 0;
        for (std::size_t i = 0; i <= tokens_.size(); ++i) {
            if (inicios & um(i)) {
                fins |= chama(regra, i);
            }
        }
        return fins;
    }

    // Consome um terminal de `tipos` a partir de cada posição.
    Posicoes terminal(Posicoes inicios, std::initializer_list<TipoToken> tipos) {
        Posicoes fins = 0;
        for (std::size_t i = 0; i < tokens_.size(); ++i) {
            if ((inicios & um(i)) && ehUm(i, tipos)) {
                fins |= um(i + 1);
            }
        }
        return fins;
    }

    // { regra } — inclui a repetição vazia.
    Posicoes repeticao(Posicoes inicios, Regra regra) {
        Posicoes todas = inicios;
        Posicoes fronteira = inicios;
        while (fronteira != 0) {
            fronteira = seguido(fronteira, regra) & ~todas;
            todas |= fronteira;
        }
        return todas;
    }

    // X { op X } — a forma de toda lista com operador entre os itens.
    Posicoes lista(std::size_t i, Regra item, std::initializer_list<TipoToken> operadores) {
        Posicoes todas = chama(item, i);
        Posicoes fronteira = todas;
        while (fronteira != 0) {
            fronteira = seguido(terminal(fronteira, operadores), item) & ~todas;
            todas |= fronteira;
        }
        return todas;
    }

    // comando = IDENT "=" expressao
    //         | "toca" expressao "por" expressao
    //         | "pausa" "por" expressao
    //         | ( "andamento" | "oitava" | "volume" ) expressao
    //         | "repita" expressao "vezes" bloco ;
    Posicoes comando(std::size_t i) {
        const Posicoes aqui = um(i);
        return seguido(terminal(terminal(aqui, {TipoToken::Identificador}),
                                {TipoToken::Atribuicao}),
                       Regra::Expressao)
             | seguido(terminal(seguido(terminal(aqui, {TipoToken::Toca}), Regra::Expressao),
                                {TipoToken::Por}),
                       Regra::Expressao)
             | seguido(terminal(terminal(aqui, {TipoToken::Pausa}), {TipoToken::Por}),
                       Regra::Expressao)
             | seguido(terminal(aqui, {TipoToken::Andamento, TipoToken::Oitava,
                                       TipoToken::Volume}),
                       Regra::Expressao)
             | seguido(terminal(seguido(terminal(aqui, {TipoToken::Repita}), Regra::Expressao),
                                {TipoToken::Vezes}),
                       Regra::Bloco);
    }

    // bloco = "{" { comando } "}" ;
    Posicoes bloco(std::size_t i) {
        return terminal(repeticao(terminal(um(i), {TipoToken::AbreChave}), Regra::Comando),
                        {TipoToken::FechaChave});
    }

    // expressao = conjuncao { "ou" conjuncao } ;
    Posicoes expressao(std::size_t i) {
        return lista(i, Regra::Conjuncao, {TipoToken::OperadorOu});
    }

    // conjuncao = relacional { "e" relacional } ;
    Posicoes conjuncao(std::size_t i) {
        return lista(i, Regra::Relacional, {TipoToken::OperadorE});
    }

    // relacional = aditiva_fechada [ op_relacional aditiva ]
    //            | aditiva_aberta ;
    Posicoes relacional(std::size_t i) {
        const Posicoes fechada = chama(Regra::AditivaFechada, i);
        const Posicoes depoisDoOperador = terminal(
            fechada, {TipoToken::Igual, TipoToken::Diferente, TipoToken::Menor,
                      TipoToken::Maior, TipoToken::MenorOuIgual, TipoToken::MaiorOuIgual});
        return fechada | seguido(depoisDoOperador, Regra::Aditiva)
             | chama(Regra::AditivaAberta, i);
    }

    // aditiva = aditiva_fechada | aditiva_aberta ;
    Posicoes aditiva(std::size_t i) {
        return chama(Regra::AditivaFechada, i) | chama(Regra::AditivaAberta, i);
    }

    // aditiva_fechada = termo_fechado { ( "+" | "-" ) termo_fechado } ;
    Posicoes aditivaFechada(std::size_t i) {
        return lista(i, Regra::TermoFechado, {TipoToken::Soma, TipoToken::Subtracao});
    }

    // aditiva_aberta = [ aditiva_fechada ( "+" | "-" ) ] termo_aberto ;
    Posicoes aditivaAberta(std::size_t i) {
        const Posicoes inicios =
            um(i) | terminal(chama(Regra::AditivaFechada, i),
                             {TipoToken::Soma, TipoToken::Subtracao});
        return seguido(inicios, Regra::TermoAberto);
    }

    // termo_fechado = unario_fechado { ( "*" | "/" ) unario_fechado } ;
    Posicoes termoFechado(std::size_t i) {
        return lista(i, Regra::UnarioFechado,
                     {TipoToken::Multiplicacao, TipoToken::Divisao});
    }

    // termo_aberto = [ termo_fechado ( "*" | "/" ) ] unario_aberto ;
    Posicoes termoAberto(std::size_t i) {
        const Posicoes inicios =
            um(i) | terminal(chama(Regra::TermoFechado, i),
                             {TipoToken::Multiplicacao, TipoToken::Divisao});
        return seguido(inicios, Regra::UnarioAberto);
    }

    // unario_fechado = "-" unario_fechado | primario ;
    Posicoes unarioFechado(std::size_t i) {
        if (eh(i, TipoToken::Subtracao)) {
            return chama(Regra::UnarioFechado, i + 1);
        }
        return chama(Regra::Primario, i);
    }

    // unario_aberto = "-" unario_aberto | "nao" relacional ;
    Posicoes unarioAberto(std::size_t i) {
        if (eh(i, TipoToken::Subtracao)) {
            return chama(Regra::UnarioAberto, i + 1);
        }
        if (eh(i, TipoToken::OperadorNao)) {
            return chama(Regra::Relacional, i + 1);
        }
        return 0;
    }

    // primario = NOTA | INTEIRO | REAL | IDENT | "verdadeiro" | "falso"
    //          | "(" expressao ")" ;
    Posicoes primario(std::size_t i) {
        if (ehUm(i, {TipoToken::Nota, TipoToken::Inteiro, TipoToken::Real,
                     TipoToken::Identificador, TipoToken::Verdadeiro,
                     TipoToken::Falso})) {
            return um(i + 1);
        }
        return terminal(seguido(terminal(um(i), {TipoToken::AbreParen}), Regra::Expressao),
                        {TipoToken::FechaParen});
    }
};

std::vector<TipoToken> tiposDe(const std::string& fonte) {
    std::vector<TipoToken> tipos;
    for (const mus::Token& token : mus::tokenize(fonte)) {
        tipos.push_back(token.tipo);
    }
    return tipos;
}

bool ebnfAceitaTipos(std::vector<TipoToken> tipos) {
    return Reconhecedor(std::move(tipos)).aceitaPrograma();
}

// Os exemplos do documento podem crescer; as cadeias geradas têm no máximo
// 7 tokens e não passam por aqui.
bool ebnfAceita(const std::string& fonte) {
    std::vector<TipoToken> tipos = tiposDe(fonte);
    REQUIRE(tipos.size() <= Reconhecedor::kMaximoTokens);
    return ebnfAceitaTipos(std::move(tipos));
}

bool bisonAceita(const std::string& fonte) {
    return mus::parseString(fonte);
}

// ---------------------------------------------------------------------------
// Geração das cadeias
// ---------------------------------------------------------------------------

// Um representante por classe de token: `x` responde por todo `primario`,
// `*` por `*` e `/`, e assim por diante. O `-` fica ao lado do `+` porque
// também é unário, e as duas leituras precisam ser exercitadas; o `<` fica ao
// lado do `==` para provar que comparações diferentes também não encadeiam.
const std::vector<std::string> kAlfabetoExpressao = {
    "x", "+", "-", "*", "==", "<", "e", "ou", "nao", "(", ")"};

// Os comandos, com uma expressão mínima (`x`, `1`, `-`) para encaixar.
const std::vector<std::string> kAlfabetoComando = {
    "toca", "pausa", "por", "andamento", "repita", "vezes",
    "{", "}", "x", "=", "1", "-"};

std::string junta(const std::vector<std::string>& alfabeto,
                  const std::vector<std::size_t>& indices) {
    std::string texto;
    for (std::size_t i : indices) {
        texto += alfabeto[i];
        texto += ' ';
    }
    return texto;
}

struct Divergencia {
    std::string fonte;
    bool ebnf;
    bool bison;
};

// Compara EBNF e Bison em todas as cadeias de 1 a `tamanhoMaximo` tokens,
// cada uma depois de `prefixo`. Devolve a primeira divergência, se houver,
// e conta quantas cadeias cada lado aceitou.
struct Comparacao {
    std::size_t cadeias = 0;
    std::size_t aceitas = 0;
    std::vector<Divergencia> divergencias;
};

Comparacao comparaTodas(const std::vector<std::string>& alfabeto,
                        std::size_t tamanhoMaximo, const std::string& prefixo) {
    // Cada símbolo passa pelo lexer uma vez só, aqui; o lado da EBNF monta a
    // cadeia de tipos direto, sem tokenizar de novo a cada uma das centenas de
    // milhares de cadeias. O Bison, que só recebe texto, tokeniza sempre.
    const std::vector<TipoToken> tiposDoPrefixo = tiposDe(prefixo);
    std::vector<TipoToken> tipoDoSimbolo;
    for (const std::string& simbolo : alfabeto) {
        const std::vector<TipoToken> tipos = tiposDe(simbolo);
        REQUIRE(tipos.size() == 1);
        tipoDoSimbolo.push_back(tipos[0]);
    }

    Comparacao resultado;
    for (std::size_t tamanho = 1; tamanho <= tamanhoMaximo; ++tamanho) {
        std::vector<std::size_t> indices(tamanho, 0);
        while (true) {
            const std::string fonte = prefixo + junta(alfabeto, indices);
            std::vector<TipoToken> tipos = tiposDoPrefixo;
            for (std::size_t i : indices) {
                tipos.push_back(tipoDoSimbolo[i]);
            }
            const bool ebnf = ebnfAceitaTipos(std::move(tipos));
            const bool bison = bisonAceita(fonte);
            ++resultado.cadeias;
            if (bison) {
                ++resultado.aceitas;
            }
            if (ebnf != bison && resultado.divergencias.size() < 5) {
                resultado.divergencias.push_back({fonte, ebnf, bison});
            }

            // Próxima cadeia, como um odômetro na base do alfabeto.
            std::size_t pos = 0;
            while (pos < tamanho && ++indices[pos] == alfabeto.size()) {
                indices[pos] = 0;
                ++pos;
            }
            if (pos == tamanho) {
                break;
            }
        }
    }
    return resultado;
}

void exigeIguais(const Comparacao& comparacao) {
    for (const Divergencia& d : comparacao.divergencias) {
        INFO("fonte: '" << d.fonte << "'");
        INFO("EBNF " << (d.ebnf ? "aceita" : "recusa") << ", Bison "
                     << (d.bison ? "aceita" : "recusa"));
        CHECK(d.ebnf == d.bison);
    }
    CHECK(comparacao.divergencias.empty());
}

// ---------------------------------------------------------------------------
// Exemplos do documento
// ---------------------------------------------------------------------------

std::string lerDocumento() {
    const std::string caminho = std::string(MUS_SOURCE_DIR) + "/docs/gramatica_ebnf.md";
    std::ifstream arquivo(caminho, std::ios::binary);
    REQUIRE_MESSAGE(arquivo.good(), "não abriu " << caminho);
    return std::string(std::istreambuf_iterator<char>(arquivo),
                       std::istreambuf_iterator<char>());
}

// Os blocos cercados por ``` cuja linha de abertura é exatamente
// "```<rotulo>".
std::vector<std::string> blocos(const std::string& documento, const std::string& rotulo) {
    std::vector<std::string> resultado;
    std::istringstream linhas(documento);
    std::string linha;
    std::string atual;
    bool dentro = false;
    while (std::getline(linhas, linha)) {
        if (!dentro && linha == "```" + rotulo) {
            dentro = true;
            atual.clear();
        } else if (dentro && linha == "```") {
            dentro = false;
            resultado.push_back(atual);
        } else if (dentro) {
            atual += linha + "\n";
        }
    }
    return resultado;
}

}  // namespace

// ---------------------------------------------------------------------------
// A EBNF e o Bison reconhecem a mesma linguagem
// ---------------------------------------------------------------------------

TEST_CASE("a EBNF do documento aceita as mesmas expressões que o parser.y") {
    // 11 símbolos, até 5 tokens: 177 155 expressões, uma por uma.
    const Comparacao comparacao = comparaTodas(kAlfabetoExpressao, 5, "y = ");
    exigeIguais(comparacao);

    // Sanidade do próprio teste: se os dois recusassem tudo, "bater" não
    // provaria nada.
    CHECK(comparacao.aceitas > 200);
}

TEST_CASE("a EBNF do documento aceita os mesmos comandos que o parser.y") {
    const Comparacao comparacao = comparaTodas(kAlfabetoComando, 5, "");
    exigeIguais(comparacao);
    CHECK(comparacao.aceitas > 50);
}

TEST_CASE("o reconhecedor da EBNF não é trivial") {
    // Cada caso aqui é uma decisão da gramática que o documento descreve. Se
    // o reconhecedor errasse algum, a comparação exaustiva acima pegaria —
    // mas é aqui que fica claro, ao ler, o que cada regra decide.
    SUBCASE("comparações não se encadeiam") {
        CHECK_FALSE(ebnfAceita("x = 1 == 1 == 1"));
        CHECK_FALSE(bisonAceita("x = 1 == 1 == 1"));
        CHECK_FALSE(ebnfAceita("x = 1 < 2 < 3"));
    }

    SUBCASE("nao à direita de um operador engole até o próximo e/ou") {
        // verdadeiro == nao (falso == verdadeiro)
        CHECK(ebnfAceita("x = verdadeiro == nao falso == verdadeiro"));
        CHECK(bisonAceita("x = verdadeiro == nao falso == verdadeiro"));
        // ...mas o que ele engoliu continua sem poder encadear comparação.
        CHECK_FALSE(ebnfAceita("x = 1 == nao 1 == 1 == 1"));
        CHECK_FALSE(bisonAceita("x = 1 == nao 1 == 1 == 1"));
        // e / ou ficam de fora: (nao a) e b.
        CHECK(ebnfAceita("x = 1 == nao a e b == c"));
    }

    SUBCASE("comandos não têm separador") {
        CHECK(ebnfAceita("toca do4 por 1 toca re4 por 1"));
        CHECK(ebnfAceita(""));
        CHECK_FALSE(ebnfAceita("toca do4 por 1,"));
    }

    SUBCASE("bloco só existe depois de repita") {
        CHECK(ebnfAceita("repita 2 vezes { }"));
        CHECK_FALSE(ebnfAceita("{ toca do4 por 1 }"));
    }

    SUBCASE("palavras reservadas sem regra são recusadas") {
        CHECK_FALSE(ebnfAceita("se x entao toca do4 por 1"));
        CHECK_FALSE(ebnfAceita("defina f"));
    }
}

TEST_CASE("os exemplos válidos do documento compilam") {
    const std::vector<std::string> validos = blocos(lerDocumento(), "mus");
    REQUIRE(validos.size() >= 5);
    for (const std::string& fonte : validos) {
        INFO("exemplo:\n" << fonte);
        std::string erro;
        CHECK_MESSAGE(mus::parseString(fonte, &erro), erro);
        CHECK(ebnfAceita(fonte));
    }
}

TEST_CASE("os exemplos inválidos do documento são recusados") {
    const std::vector<std::string> invalidos = blocos(lerDocumento(), "mus-invalido");
    REQUIRE(invalidos.size() >= 5);
    for (const std::string& fonte : invalidos) {
        INFO("exemplo:\n" << fonte);
        CHECK_FALSE(mus::parseString(fonte));
        CHECK_FALSE(ebnfAceita(fonte));
    }
}
