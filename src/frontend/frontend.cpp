#include "frontend/frontend.h"

#include <cstdlib>
#include <cstring>

#include "frontend/scanner.h"

// Interface com o código gerado pelo Flex e pelo Bison. Declarada aqui, e não
// incluída de `lex.yy.cpp`/`parser.tab.h`, para que este arquivo não dependa do
// diretório de build — quem inclui `frontend.h` não precisa saber que existe
// geração de código por baixo.
struct yy_buffer_state;
typedef yy_buffer_state* YY_BUFFER_STATE;

extern YY_BUFFER_STATE yy_scan_string(const char* texto);
extern void yy_delete_buffer(YY_BUFFER_STATE buffer);
extern int yylex();
extern int yyparse();

namespace mus {
namespace scanner {

Token tokenAtual;
std::vector<ast::Comando*>* arvoreAtual = nullptr;
std::string erroSintatico;

namespace {

// Posição do próximo caractere ainda não consumido. Ambas começam em 1 porque é
// assim que editor e compilador contam.
int proximaLinha = 1;
int proximaColuna = 1;

// Semitom de cada nota dentro da oitava, no sistema temperado.
struct NomeDeNota {
    const char* nome;
    int semitom;
};

// Ordem importa: `sol` precisa ser testado antes de `so`... que não existe, mas
// o princípio vale para qualquer nome que seja prefixo de outro. Manter os de
// 3 letras primeiro deixa a busca correta por construção.
const NomeDeNota kNotas[] = {
    {"sol", 7}, {"do", 0}, {"re", 2}, {"mi", 4}, {"fa", 5}, {"la", 9}, {"si", 11},
};

}  // namespace

void reiniciar() {
    proximaLinha = 1;
    proximaColuna = 1;
    tokenAtual = Token();
    arvoreAtual = nullptr;
    erroSintatico.clear();
}

void avancarPosicao(const char* texto, int tamanho) {
    tokenAtual.linha = proximaLinha;
    tokenAtual.coluna = proximaColuna;

    for (int i = 0; i < tamanho; ++i) {
        if (texto[i] == '\n') {
            ++proximaLinha;
            proximaColuna = 1;
        } else {
            ++proximaColuna;
        }
    }
}

void registrarToken(TipoToken tipo, const char* texto, int tamanho) {
    tokenAtual.tipo = tipo;
    tokenAtual.lexema.assign(texto, static_cast<std::size_t>(tamanho));
    tokenAtual.valor = 0.0;
    tokenAtual.semitom = -1;
    tokenAtual.oitava = -1;
    tokenAtual.midi = -1;
}

void registrarNumero(TipoToken tipo, const char* texto, int tamanho) {
    registrarToken(tipo, texto, tamanho);
    tokenAtual.valor = std::strtod(tokenAtual.lexema.c_str(), 0);
}

void registrarNota(const char* texto, int tamanho) {
    registrarToken(TipoToken::Nota, texto, tamanho);
    analisarNota(tokenAtual.lexema, &tokenAtual.semitom, &tokenAtual.oitava,
                 &tokenAtual.midi);
}

bool analisarNota(const std::string& lexema, int* semitom, int* oitava, int* midi) {
    *semitom = -1;
    *oitava = -1;
    *midi = -1;

    std::size_t posicao = 0;
    for (std::size_t i = 0; i < sizeof(kNotas) / sizeof(kNotas[0]); ++i) {
        const std::size_t tamanho = std::strlen(kNotas[i].nome);
        if (lexema.compare(0, tamanho, kNotas[i].nome) == 0) {
            *semitom = kNotas[i].semitom;
            posicao = tamanho;
            break;
        }
    }

    if (*semitom < 0) {
        return false;
    }

    if (posicao < lexema.size() && lexema[posicao] == '#') {
        *semitom += 1;
        ++posicao;
    } else if (posicao < lexema.size() && lexema[posicao] == 'b') {
        *semitom -= 1;
        ++posicao;
    }

    if (posicao < lexema.size() && lexema[posicao] >= '0' && lexema[posicao] <= '9') {
        *oitava = lexema[posicao] - '0';
        ++posicao;
        // Notação científica de altura: dó 4 é o dó central, a nota MIDI 60, e
        // lá 4 cai em 69 — o kMidiA4 de `core/pitch.h`, ou seja 440 Hz.
        *midi = (*oitava + 1) * 12 + *semitom;
    }

    return posicao == lexema.size();
}

}  // namespace scanner

const char* nomeToken(TipoToken tipo) {
    switch (tipo) {
        case TipoToken::FimDeArquivo:  return "FIM";
        case TipoToken::Nota:          return "NOTA";
        case TipoToken::Inteiro:       return "INTEIRO";
        case TipoToken::Real:          return "REAL";
        case TipoToken::Identificador: return "IDENT";
        case TipoToken::Toca:          return "TOCA";
        case TipoToken::Pausa:         return "PAUSA";
        case TipoToken::Por:           return "POR";
        case TipoToken::Andamento:     return "ANDAMENTO";
        case TipoToken::Oitava:        return "OITAVA";
        case TipoToken::Volume:        return "VOLUME";
        case TipoToken::Repita:        return "REPITA";
        case TipoToken::Vezes:         return "VEZES";
        case TipoToken::Se:            return "SE";
        case TipoToken::Entao:         return "ENTAO";
        case TipoToken::Senao:         return "SENAO";
        case TipoToken::Defina:        return "DEFINA";
        case TipoToken::Retorna:       return "RETORNA";
        case TipoToken::Verdadeiro:    return "VERDADEIRO";
        case TipoToken::Falso:         return "FALSO";
        case TipoToken::OperadorE:     return "E";
        case TipoToken::OperadorOu:    return "OU";
        case TipoToken::OperadorNao:   return "NAO";
        case TipoToken::Soma:          return "SOMA";
        case TipoToken::Subtracao:     return "SUBTRACAO";
        case TipoToken::Multiplicacao: return "MULTIPLICACAO";
        case TipoToken::Divisao:       return "DIVISAO";
        case TipoToken::Igual:         return "IGUAL";
        case TipoToken::Diferente:     return "DIFERENTE";
        case TipoToken::Menor:         return "MENOR";
        case TipoToken::MenorOuIgual:  return "MENOR_IGUAL";
        case TipoToken::Maior:         return "MAIOR";
        case TipoToken::MaiorOuIgual:  return "MAIOR_IGUAL";
        case TipoToken::Atribuicao:    return "ATRIBUICAO";
        case TipoToken::AbreParen:     return "ABRE_PAREN";
        case TipoToken::FechaParen:    return "FECHA_PAREN";
        case TipoToken::AbreChave:     return "ABRE_CHAVE";
        case TipoToken::FechaChave:    return "FECHA_CHAVE";
        case TipoToken::Virgula:       return "VIRGULA";
        case TipoToken::Desconhecido:  return "DESCONHECIDO";
    }
    return "DESCONHECIDO";
}

std::vector<Token> tokenize(const std::string& fonte) {
    std::vector<Token> tokens;

    scanner::reiniciar();
    YY_BUFFER_STATE buffer = yy_scan_string(fonte.c_str());

    while (yylex() != 0) {
        tokens.push_back(scanner::tokenAtual);
    }

    yy_delete_buffer(buffer);
    return tokens;
}

bool parseString(const std::string& fonte, std::string* erro) {
    scanner::reiniciar();
    YY_BUFFER_STATE buffer = yy_scan_string(fonte.c_str());

    const bool aceito = (yyparse() == 0);

    yy_delete_buffer(buffer);

    if (erro != 0) {
        *erro = scanner::erroSintatico;
    }
    return aceito;
}

std::vector<ast::Comando*>* parseToAst(const std::string& fonte, std::string* erro) {
    scanner::reiniciar();
    YY_BUFFER_STATE buffer = yy_scan_string(fonte.c_str());

    const bool aceito = (yyparse() == 0);

    yy_delete_buffer(buffer);

    if (erro != 0) {
        *erro = scanner::erroSintatico;
    }
    // `programa: lista_comandos` só grava em `arvoreAtual` quando a redução
    // final roda, então um erro de sintaxe deixa `arvoreAtual` em nullptr —
    // não precisa de checagem redundante contra `aceito`.
    return aceito ? scanner::arvoreAtual : nullptr;
}

}  // namespace mus
