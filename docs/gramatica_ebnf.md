---
title: Gramática EBNF
layout: default
parent: Documentação
nav_order: 9
---

# Gramática da Linguagem (EBNF)

Este documento é a definição formal da linguagem musical: o que o analisador léxico (`lexer/lexer.l`, Flex) transforma em tokens e o que o analisador sintático (`parser/parser.y`, Bison) aceita como programa. Ele descreve **o compilador como ele é hoje**, e não a linguagem planejada: o que ainda não tem regra no `parser.y` está separado na seção 4.

A afirmação "esta gramática descreve o compilador" é verificada por teste, e não só revisada à mão: ver a seção 6.

{: .nota }
> **Notação.** `=` define uma regra, `|` separa alternativas, `[ x ]` é opcional, `{ x }` repete zero ou mais vezes, `( x )` agrupa, `"x"` é um terminal literal e `;` fecha a regra. Nomes em MAIÚSCULAS (`NOTA`, `IDENT`, ...) são os tokens definidos na seção 1.

---

## 1. Léxico

### 1.1 O que é ignorado

```ebnf
espaco             = " " | "\t" | "\r" | "\n" ;
comentario_linha   = "//" { qualquer caractere exceto "\n" } ;
comentario_bloco   = "/*" { qualquer caractere } "*/" ;   (* não aninha *)
```

A quebra de linha **não tem significado**: um comando não precisa terminar no fim da linha, e não existe `;` entre comandos (ver 2.1). O comentário de bloco termina no primeiro `*/`, então `/* a /* b */ c */` deixa o `c */` de fora, e isso é erro de sintaxe.

### 1.2 Palavras reservadas

| Grupo | Palavras |
|---|---|
| Comandos | `toca` `pausa` `por` `andamento` `oitava` `volume` |
| Repetição | `repita` `vezes` |
| Lógica | `e` `ou` `nao` (ou `não`) `verdadeiro` `falso` |
| Reservadas, ainda sem regra | `se` `entao` (ou `então`) `senao` (ou `senão`) `defina` `retorna` |

As formas com acento são sinônimos exatos das sem acento: o teclado do usuário não decide se o programa compila. Nenhuma palavra reservada pode ser nome de variável. `tempo` **não** é reservada: é um identificador comum.

### 1.3 Tokens

```ebnf
letra       = "a" | ... | "z" | "A" | ... | "Z" | "_" ;
digito      = "0" | ... | "9" ;

NOTA        = nome_nota [ acidente ] [ digito ] ;
nome_nota   = "do" | "re" | "mi" | "fa" | "sol" | "la" | "si" ;
acidente    = "#" | "b" ;                (* sustenido, bemol *)

INTEIRO     = digito { digito } ;
REAL        = digito { digito } "." digito { digito } ;
IDENT       = letra { letra | digito } ;  (* exceto palavras reservadas e notas: ver 1.4 *)
STRING      = '"' { qualquer caractere exceto '"' e "\n" } '"' ;   (* nome de sample, INT-05 *)

op_relacional = "==" | "!=" | "<" | ">" | "<=" | ">=" ;
```

Os demais terminais são os caracteres `+ - * / = ( ) { } ,`, lidos um a um. A vírgula só aparece numa regra: a que separa as trilhas de um grupo (seção 2.1).

`STRING` é um texto entre aspas duplas, numa linha só e sem aspas escapadas. Ele entrou com a INT-05 (#31, PR #137) para nomear um sample WAV da pasta `samples/`, como em `toca "PERC_09" por 1`; a AST guarda o texto sem as aspas.

O dígito da `NOTA` é a oitava, colado ao nome: `do4` é o dó central (MIDI 60). Sem ele (`do`, `fa#`), vale a oitava corrente, definida pelo comando `oitava`. `INTEIRO` e `REAL` são tokens distintos, embora a AST guarde os dois como número.

### 1.4 Desambiguação léxica

Várias regras acima casam o mesmo texto: `do4` é tanto `NOTA` quanto `IDENT`, e `repita` é tanto palavra reservada quanto `IDENT`. O Flex decide por duas regras, nesta ordem:

1. **Casamento mais longo:** vence a regra que consome mais caracteres.
2. **Empate:** vence a regra declarada primeiro no `lexer.l`, cuja ordem é: palavras reservadas, `NOTA`, `STRING`, `REAL`, `INTEIRO`, `IDENT`, operadores. O `STRING` nunca empata com outra regra, porque só ele começa com aspas.

Consequências que o usuário percebe:

| Texto | Vira | Por quê |
|---|---|---|
| `do4`, `sib3`, `fa#` | `NOTA` | empate com `IDENT`, e `NOTA` vem antes |
| `dose`, `solo`, `do45` | `IDENT` | `IDENT` casa mais caracteres |
| `do10` | `IDENT` | a oitava tem **um** dígito, então `IDENT` casa mais |
| `do`, `fab`, `sol4` | `NOTA` | não podem ser nomes de variável: `fab = 1` é erro de sintaxe |
| `repitax` | `IDENT` | mais longo que `repita` |
| `<=` | um token | mais longo que `<` seguido de `=` |
| `1.` e `.5` | erro | `REAL` exige dígito dos dois lados do ponto |
| `dó4`, `ç` | erro | notas e identificadores são só ASCII; acento só nas três palavras-chave da tabela 1.2 |

Um caractere que não casa com nenhuma regra (`@`, `;`, `#` solto) não interrompe o lexer. Ele vira um token desconhecido, e o parser o recusa com linha e coluna.

---

## 2. Sintaxe

### 2.1 Programa e comandos

```ebnf
programa = { comando } ;

comando  = IDENT "=" expressao                     (* atribuição *)
         | "toca" expressao "por" expressao        (* altura, duração em tempos *)
         | "pausa" "por" expressao
         | "andamento" expressao                   (* BPM *)
         | "oitava" expressao
         | "volume" expressao                      (* 0 a 1 *)
         | "repita" expressao "vezes" bloco
         | grupo ;                                 (* trilhas paralelas *)

bloco    = "{" { comando } "}" | grupo ;

grupo    = "{" trilha "," trilha { "," trilha } "}" ;
trilha   = { comando } ;
```

- **Programa vazio é válido**: um arquivo só com comentários compila e não toca nada.
- **Não há separador entre comandos.** Cada comando começa com uma palavra reservada ou com `IDENT "="`, e nenhuma expressão pode continuar com esses tokens, então o parser sabe onde um comando termina. Por isso `toca do4 por 1 toca re4 por 1`, numa linha só, são dois comandos.
- **`por` é obrigatório** em `toca` e `pausa`. Sem ele, `toca la4 - 2` seria ambíguo: "lá 4 menos 2 semitons" ou "lá 4 por −2 tempos"?
- **Chave com vírgula é grupo; chave sem vírgula é bloco.** O grupo é um comando por si (`{ toca do4 por 1, toca mi4 por 1 }`) e também pode ser o corpo de um `repita`, que então repete as trilhas juntas. O bloco sem vírgula só existe depois de `repita`: `{ toca do4 por 1 }` solto é erro.
- **Trilhas paralelas (CMP-07, #39).** Todas as trilhas de um grupo começam no mesmo instante, e o grupo dura o que dura a mais longa. As durações escritas valem como sempre, sem reescala. Uma trilha pode ser vazia, como o bloco vazio do `repita`, então `{ a, }` é o mesmo que `a`. O que acontece com o tempo, a oitava e o volume está na [Representação Intermediária]({{ site.baseurl }}{% link representacao_intermediaria.md %}).
- **Sem conflito.** `bloco` e `grupo` começam igual (`"{" { comando }`), e só o token seguinte decide qual dos dois é: `}` fecha o bloco, `,` abre a segunda trilha. O `parser.y` declara `%expect 0`, então um conflito shift/reduce que alguém introduza no futuro quebra o build em vez de virar um aviso.

### 2.2 Expressões, na forma do `parser.y`

O `parser.y` escreve as expressões numa gramática **ambígua** e resolve a ambiguidade com declarações de precedência (`%left`, `%right`, `%nonassoc`). É a forma que o Bison lê:

```ebnf
expressao = expressao op_binario expressao
          | "nao" expressao
          | "-" expressao
          | "(" expressao ")"
          | primario ;

op_binario = "ou" | "e" | op_relacional | "+" | "-" | "*" | "/" ;

primario   = NOTA | INTEIRO | REAL | STRING | IDENT | "verdadeiro" | "falso" ;
```

| Precedência | Operadores | Associatividade |
|:-:|---|---|
| 1 (menor) | `ou` | esquerda |
| 2 | `e` | esquerda |
| 3 | `nao` | prefixo |
| 4 | `==` `!=` `<` `>` `<=` `>=` | **não associativos** |
| 5 | `+` `-` | esquerda |
| 6 | `*` `/` | esquerda |
| 7 (maior) | `-` unário | prefixo |

A gramática não tem nenhum conflito shift/reduce ou reduce/reduce que o Bison precise resolver sozinho: todos são resolvidos por essa tabela. Na prática:

- `a - b - c` é `(a - b) - c`, e `-a * b` é `(-a) * b`;
- `1 == 1 == 1` e `1 < 2 < 3` são **erro de sintaxe**, porque as comparações são não associativas;
- `nao a == b` é `nao (a == b)`, e `nao a e b` é `(nao a) e b`;
- `la4 - 2` é a nota `la4` transposta 2 semitons para baixo. Que isso só valha para `nota - numero` é regra de **tipo**, da análise semântica, e não da gramática (ver 2.5).

### 2.3 O caso do `nao`

O `nao` tem precedência **menor** que as comparações, para que `nao x == y` signifique `nao (x == y)`. O efeito colateral, típico de geradores yacc, aparece quando o `nao` está **à direita** de outro operador. Ali ele começa uma subexpressão que engole tudo o que vem depois, até o próximo `e`, `ou` ou o fim da expressão:

| Programa | O parser lê |
|---|---|
| `verdadeiro == nao falso == verdadeiro` | `verdadeiro == nao (falso == verdadeiro)` |
| `1 + nao x + 2` | `1 + nao (x + 2)` |
| `a == nao b e c` | `(a == (nao b)) e c` |

O primeiro exemplo é aceito mesmo com duas comparações seguidas, porque a segunda ficou dentro do `nao`. O segundo passa no parser e é recusado na análise semântica (`numero + booleano`).

O comportamento foi **mantido**. Recusá-lo exigiria reescrever as expressões do `parser.y` em níveis, como na seção 2.4, o que só muda a resposta para programas com `nao` no meio de uma conta, e nenhum exemplo do projeto faz isso. Ele fica registrado aqui porque a gramática precisa descrever o compilador real, e porque uma EBNF ingênua, com um nível por linha da tabela, **não** descreve esse comportamento: ela recusaria o primeiro exemplo.

### 2.4 Expressões, na forma estratificada (sem ambiguidade)

A mesma linguagem da seção 2.2, escrita com um não-terminal por nível de precedência. Ela não precisa da tabela, porque a precedência está na forma das regras: quanto mais funda a regra, mais forte o operador.

```ebnf
expressao        = conjuncao { "ou" conjuncao } ;
conjuncao        = relacional { "e" relacional } ;

relacional       = aditiva_fechada [ op_relacional aditiva ]
                 | aditiva_aberta ;

aditiva          = aditiva_fechada | aditiva_aberta ;
aditiva_fechada  = termo_fechado { ( "+" | "-" ) termo_fechado } ;
aditiva_aberta   = [ aditiva_fechada ( "+" | "-" ) ] termo_aberto ;

termo_fechado    = unario_fechado { ( "*" | "/" ) unario_fechado } ;
termo_aberto     = [ termo_fechado ( "*" | "/" ) ] unario_aberto ;

unario_fechado   = "-" unario_fechado | primario ;
unario_aberto    = "-" unario_aberto | "nao" relacional ;

primario         = NOTA | INTEIRO | REAL | STRING | IDENT | "verdadeiro" | "falso"
                 | "(" expressao ")" ;
```

**Fechada** é a expressão que não contém nenhum `nao` fora de parênteses. **Aberta** é a que termina num `nao` ainda sem fechar, o da seção 2.3: como ele já consumiu o resto do nível relacional, uma expressão aberta só pode ser o **último** operando, e depois dela só vem `e`, `ou` ou o fim. Repare que `"nao" relacional` não aparece em `conjuncao` diretamente: um `nao` no começo da expressão é só o caso de `aditiva_aberta` sem nada antes.

Como as regras codificam a tabela:

- **associatividade à esquerda**: as repetições `{ op X }` são lidas da esquerda para a direita, então `a - b - c` agrupa como `(a - b) - c`;
- **comparação não associativa**: o `[ op_relacional aditiva ]` é opcional, e não uma repetição, então só cabe uma comparação por nível;
- **o `-` unário é o mais forte**: ele fica no nível mais fundo, junto do `primario`.

### 2.5 O que fica fora da gramática

Três verificações acontecem depois da gramática, ou ao lado dela, e não são livres de contexto:

- **Profundidade (CMP-08, #44):** o parser recusa uma árvore com mais de **256 níveis**. Expressões compostas, `repita` dentro de `repita` e grupo dentro de grupo contam; parênteses não. A verificação está nas ações das regras, e não nas produções. O erro é `aninhamento passa do limite de 256 níveis`.
- **Tipos e escopo (CMP-13, #95):** `toca` exige altura `nota` (ou `numero`, lido como MIDI) e duração `numero`; `nota + nota` é inválido; uma variável precisa ser atribuída antes do uso; cada trilha abre o próprio escopo, não pode reatribuir variável declarada fora do grupo, e `andamento` não pode aparecer dentro de grupo. Ver a tabela de tipos na [Especificação de Requisitos]({{ site.baseurl }}{% link especificacao.md %}).
- **Faixas de valor:** `volume` entre 0 e 1 e `andamento` positivo são conferidos pelo interpretador, ao gerar a [Representação Intermediária]({{ site.baseurl }}{% link representacao_intermediaria.md %}).

---

## 3. Exemplos

Todos os blocos desta seção são compilados pela suíte de testes (seção 6). Se a gramática mudar e um exemplo deixar de valer, o teste falha.

### 3.1 Programas válidos

O programa de referência do projeto (`exemplos/arpejo.mus`):

```mus
andamento 120
volume 0.8
tempo = 0.5

repita 4 vezes {
    toca do4  por tempo
    toca mi4  por tempo
    toca sol4 por tempo
}

toca la4 - 2 por tempo * 2    /* transposição e duração calculadas */
pausa por 1.0
```

Notas sem oitava, com sustenido e bemol, e dois comandos na mesma linha:

```mus
oitava 3
toca do por 1 toca fa# por 0.5
toca sib3 por 0.5   // si bemol 3
```

Precedência e associatividade: `(-2 * 3) + 4`, e `(nao ((1 + 2) == 3)) e verdadeiro`:

```mus
x = -2 * 3 + 4
ok = nao 1 + 2 == 3 e verdadeiro
```

O `nao` à direita de uma comparação (seção 2.3), e a forma com acento:

```mus
y = verdadeiro == nao falso == verdadeiro
desligado = não verdadeiro
```

Repetição aninhada, com bloco vazio:

```mus
repita 2 vezes {
    repita 3 vezes { toca do4 por 0.25 }
    repita 1 vezes { }
}
```

Trilhas paralelas: três contra quatro no mesmo tempo, repetidas, e um acorde:

```mus
repita 2 vezes {
    repita 3 vezes { toca do4 por 1 / 3 },
    repita 4 vezes { toca sol3 por 1 / 4 }
}
{ toca do4 por 2, toca mi4 por 2, toca sol4 por 2 }
```

Um grupo dentro de uma trilha, e uma trilha vazia no fim:

```mus
{
    { toca do4 por 1, toca mi4 por 1 }
    toca sol4 por 1,
    pausa por 2,
}
```

Um sample WAV pelo nome (INT-05), direto e por variável, ao lado de uma nota:

```mus
toca "PERC_09" por 1
instrumento = "BASS_1"
toca instrumento por 2
toca do4 por 1
```

Um programa sem comandos:

```mus
// só comentários
/* também
   valem */
```

### 3.2 Programas inválidos

Sem o `por`, a duração não tem como ser separada da altura:

```mus-invalido
toca do4 0.5
```

Comparações não se encadeiam:

```mus-invalido
x = 1 == 1 == 1
```

`fab` é a nota fá bemol, e não pode ser nome de variável (seção 1.4):

```mus-invalido
fab = 1
```

Chave sem vírgula só existe como bloco de `repita`, e o `vezes` é obrigatório:

```mus-invalido
{ toca do4 por 1 }
```

```mus-invalido
repita 2 { toca do4 por 1 }
```

A vírgula só separa trilhas dentro de chaves:

```mus-invalido
toca do4 por 1, toca mi4 por 1
```

Grupo sem fechar:

```mus-invalido
{ toca do4 por 1, toca mi4 por 1
```

Real sem dígito antes do ponto:

```mus-invalido
x = .5
```

Condicional ainda não tem regra (seção 4):

```mus-invalido
se x entao toca do4 por 1
```

Separador de comando não existe:

```mus-invalido
toca do4 por 1;
toca re4 por 1
```

Parêntese sem fechar:

```mus-invalido
x = (1 + 2
```

---

## 4. Construções reservadas, ainda sem regra

O lexer já reconhece estes tokens, mas o `parser.y` não tem nenhuma produção que os use. Qualquer programa que os contenha é **erro de sintaxe**:

| Token | Para quê |
|---|---|
| `se`, `entao`, `senao` | condicional |
| `defina`, `retorna` | funções |

A vírgula, que estava nesta tabela para os parâmetros das funções, ganhou regra com as trilhas paralelas. Parâmetros futuros podem reusá-la dentro de parênteses sem conflito, porque ali o parser já sabe que não está entre as trilhas de um grupo.

As duas construções estão entre os requisitos planejados da [Especificação de Requisitos]({{ site.baseurl }}{% link especificacao.md %}).

Reservar a palavra antes da regra existir impede que um programa de hoje use `se` como nome de variável e quebre quando a condicional entrar.

A mini-notação rítmica do modelo de padrões infinitos (`[ ]` e `*` como multiplicador de padrão) **não** faz parte da linguagem: ela saiu do escopo na decisão de 16/09 pelo modelo de programa finito (ver a [Visão de Arquitetura]({{ site.baseurl }}{% link arquitetura.md %})). O `{ , }` daquela mini-notação voltou com outro significado em 02/11 (CMP-07, #39): no Tidal ele é polimetria, com cada padrão reescalado para caber num ciclo que se repete para sempre; aqui são trilhas finitas, com as durações escritas valendo como estão.

---

## 5. Correspondência com o código

| Seção | Onde está no código |
|---|---|
| 1.1 a 1.4, léxico | `lexer/lexer.l`, na ordem das regras |
| 2.1, `programa`, `comando`, `bloco`, `grupo`, `trilha` | `parser/parser.y`, regras `programa`, `lista_comandos`, `comando`, `bloco`, `grupo` e `trilhas` |
| 2.2, expressões e precedência | `parser/parser.y`, regra `expressao` e declarações `%left`/`%right`/`%nonassoc` |
| 2.4, forma estratificada | não existe no código; é transcrita em `tests/test_gramatica.cpp` |
| 2.5, profundidade | macro `LIMITA_PROFUNDIDADE` em `parser/parser.y` e `kProfundidadeMaxima` em `src/interpreter/ast.h` |

Diferenças de nome: `lista_comandos` do `parser.y` é o `{ comando }` daqui, e também a `trilha`; `trilhas` é a lista das trilhas depois da primeira vírgula. `primario` não existe lá como regra separada, porque os literais são alternativas diretas de `expressao`.

---

## 6. Como a gramática é verificada

`tests/test_gramatica.cpp` transcreve a forma estratificada da seção 2.4, regra por regra, num reconhecedor que lê a EBNF como definição de conjunto: cada regra devolve todas as posições onde pode terminar, sem nenhuma regra de desempate. Depois, o teste:

1. **compara o reconhecedor com o Bison em todas as expressões de até 5 tokens** montadas com um representante de cada classe de token (`x + - * == < e ou nao ( )`): são 177 155 expressões, e as respostas precisam bater uma a uma;
2. faz o mesmo para **sequências de comandos** de até 5 tokens (`toca pausa por andamento repita vezes { } , x = 1 -`): são 402 233 cadeias;
3. faz o mesmo para a **estrutura das chaves**, com cada comando valendo um símbolo só (`{ } , [pausa por 1] [repita 2 vezes]`), em todas as cadeias de até 7 símbolos: são 97 655. É esta comparação que alcança grupos válidos, porque o menor grupo com comando dentro, `{ x = 1 , x = 1 }`, já tem 8 tokens. Com 7 símbolos cabem grupo dentro de grupo, grupo como corpo de `repita` e trilha vazia;
4. **compila todos os blocos da seção 3** deste documento: os válidos precisam passar no parser e no reconhecedor, e os inválidos precisam ser recusados pelos dois.

O item 1 é o que garante as seções 2.3 e 2.4. Ele foi conferido também no sentido contrário: com o reconhecedor trocado por uma EBNF ingênua, com um nível por linha da tabela de precedência e o `nao` só no nível dele, o teste falha e aponta `y = - nao x` e `y = x + nao x`, que o Bison aceita e a versão ingênua recusa.

Os detalhes do léxico (tabela 1.4) são cobertos por `tests/test_lexer.cpp`, e as mensagens de erro com linha e coluna, por `tests/test_parser.cpp`.

**Ao mudar a sintaxe:** atualize este documento junto com o `parser.y`, e acrescente ao reconhecedor do teste a regra nova. Se o alfabeto do teste não tiver um token da construção nova, acrescente-o também; senão a comparação não a enxerga.
