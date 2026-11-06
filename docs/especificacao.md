---
title: Especificação de Requisitos
layout: default
parent: Documentação
nav_order: 6
---

# Especificação de Requisitos

## 1. Objetivo do Compilador
O projeto consiste no desenvolvimento de uma **Domain-Specific Language (DSL) focada na composição e execução de áudio**. O objetivo é permitir que o usuário escreva instruções textuais (notas, ritmos, andamentos) num arquivo de texto puro, que será lido em lote e validado pelo nosso compilador, gerando a reprodução sonora correspondente.

## 2. Arquitetura e Ferramentas
O projeto está sendo construído em **C++17** e utiliza as seguintes ferramentas de arquitetura de compiladores clássicas:
* **Flex:** Responsável pela Análise Léxica (reconhecimento dos tokens musicais, comandos, números e símbolos).
* **Bison:** Responsável pela Análise Sintática (validação das regras gramaticais e resolução de conflitos shift/reduce).
* **miniaudio:** Biblioteca C/C++ externa, responsável pelo motor de áudio. É ela que processa a reprodução dos sons após a validação completa do código-fonte.

## 3. Especificação da Linguagem (Sintaxe Base)
A linguagem musical foi desenhada de forma a ser o menos ambígua possível, utilizando vocabulário em português. As decisões de sintaxe e léxico consolidadas no parser são as seguintes (a definição formal, regra por regra, está na [Gramática EBNF]({{ site.baseurl }}{% link gramatica_ebnf.md %})):

* **Notas Musicais:** A linguagem reconhece a notação latina padrão (`do`, `re`, `mi`, `fa`, `sol`, `la`, `si`), incluindo sustenidos (`#`) e bemóis (`b`).
* **Oitavas:** Opcionais, mas quando usadas, devem estar coladas ao nome da nota (ex: `do4`). Se for omitida, o compilador adota a oitava corrente.
* **Separador de Duração:** Para evitar a ambiguidade matemática do hífen (como em `la4 - 2`), a linguagem exige obrigatoriamente a palavra-chave `por` para separar a ação da sua duração (ex: `toca do4 por tempo`).
* **Duração:** Medida em "tempos" (floats), compatíveis com o *Playhead* do motor de áudio.
* **Comandos de Estado:** Instruções diretas que alteram o estado global do interpretador sem sinal de atribuição (ex: `andamento 120`, `oitava 4`, `volume 0.8`).
* **Agrupamento de Blocos:** As estruturas compostas (como repetições) devem ser fechadas entre chaves `{ }`.
* **Trilhas Paralelas:** Chaves com vírgula formam um grupo de trilhas que tocam ao mesmo tempo (ex: `{ toca do4 por 1, toca mi4 por 1 }`). Cada trilha é uma sequência de comandos; o grupo dura o que dura a trilha mais longa, e a música continua do fim dela.
* **Comentários:** O compilador ignora comentários iniciados por `//` em qualquer posição da linha, além de blocos contidos entre `/*` e `*/`.

**Exemplo Oficial de Instrução Válida:**
O bloco de código abaixo é a representação oficial das capacidades da linguagem, presente (com comentários) no arquivo `exemplos/arpejo.mus` e totalmente aceita pela gramática atual.

```text
andamento 120
tempo = 0.5

repita 4 vezes {
    toca do4  por tempo
    toca mi4  por tempo
    toca sol4 por tempo
}

toca la4 - 2 por tempo * 2    /* transposição e duração calculadas */
pausa por 1.0
```
## 4. Estruturas Lógicas e de Controle
Com base no escopo definido para a versão P1, as capacidades lógicas do compilador dividem-se em funcionalidades já validadas pelo analisador sintático e estruturas futuras.

### Requisitos Implementados (Escopo P1)
* **Variáveis Customizadas:** Declaração e atribuição de valores utilizando o sinal de igualdade (ex: `tempo = 0.5`).
* **Laços de Repetição:** Suporte exclusivo ao ciclo definido utilizando a sintaxe repita `N vezes { ... }`. A palavra-chave "vezes" é obrigatória nesta estrutura.
* **Trilhas Paralelas (CMP-07, #39):** `{ trilha, trilha, ... }` faz várias sequências de comandos começarem no mesmo instante, o que dá acordes e polirritmias (ex: três notas `por 1 / 3` contra quatro `por 1 / 4` no mesmo tempo). As durações escritas valem como em qualquer outro lugar, sem reescala. `oitava` e `volume` mudados numa trilha valem só dentro dela, e o corpo de um `repita` pode ser um grupo.
* **Expressões e Operações:** A linguagem suporta operações aritméticas básicas (`+, -, *, /`) permitindo cálculos dinâmicos em tempo de compilação, como a transposição de notas (ex: `toca la4 - 2 por tempo * 2`). Também suporta operadores lógicos e relacionais (`e, ou, nao, verdadeiro, falso, comparações`).

### Requisitos Planejados (Próximas Sprints)
* **Estruturas Condicionais:** As palavras-chave `se`, `entao` e `senao` já são reconhecidas pelo analisador léxico, mas as regras gramaticais e os nós da Árvore Sintática (AST) correspondentes ficam para as próximas sprints.
* **Funções:** Os tokens `defina` e `retorna` já estão mapeados no Flex; a implementação de escopo e de chamadas está planejada para iterações futuras.

### Análise Semântica e Tabela de Símbolos
Entre o parser e o interpretador, a análise semântica percorre a AST e monta a **tabela de símbolos**. Para cada variável, a tabela guarda o nome, o tipo, o nível de escopo, o endereço (um *slot* sequencial) e as posições da declaração e de cada uso. A flag `--simbolos` imprime essa tabela.

* **Declaração:** a primeira atribuição a um nome declara a variável no escopo atual, com o tipo do valor. Uma reatribuição com **outro tipo** é *redeclaração inválida* (`tempo = 0.5` e depois `tempo = verdadeiro`).
* **Escopo:** o bloco `{ ... }` do `repita` abre um escopo. Uma variável criada dentro do bloco não existe depois dele. Uma variável de fora é visível dentro do bloco, e atribuir a ela atualiza a de fora.
* **Trilhas:** cada trilha de um grupo abre o próprio escopo. Ela lê as variáveis de fora do grupo, mas **não pode reatribuí-las**, porque a trilha seguinte veria a mudança só por ter sido escrita depois. Pelo mesmo motivo, `andamento` dentro de um grupo é erro: as trilhas dividem o mesmo pulso.
* **Tipos:** `numero` (inteiro ou real), `nota`, `booleano` e `string` (texto entre aspas, que nomeia um sample; INT-05).

| Expressão | Resultado |
|---|---|
| `numero + - * / numero` | `numero` |
| `nota + numero`, `numero + nota`, `nota - numero` | `nota` (transposição) |
| `nota - nota` | `numero` (intervalo em semitons) |
| `==`, `!=` entre operandos do mesmo tipo | `booleano` |
| `<`, `>`, `<=`, `>=` entre dois `numero` ou duas `nota` | `booleano` |
| `e`, `ou`, `nao` sobre `booleano` | `booleano` |
| `-x` sobre `numero` | `numero` |

`toca` exige altura `nota` (ou `numero`, lido como MIDI) e duração `numero`. `pausa`, `andamento`, `oitava`, `volume` e `repita ... vezes` exigem `numero`.

**Samples (INT-05, PR #137, em revisão).** A altura do `toca` também aceita uma `string` com o nome de um arquivo da pasta `samples/`, sem a extensão: `toca "PERC_09" por 1` toca o WAV `samples/PERC_09.wav` pela duração pedida. A `string` também pode ir para uma variável e ser comparada com `==` e `!=`; qualquer outra operação com ela é erro semântico, e um nome que não existe na pasta é recusado ao gerar a Timeline. O recurso entrou na `main` em 04/11 e ainda passa por revisão pós-merge (ver o [Roadmap]({{ site.baseurl }}{% link roadmap.md %})).

## 5. Tratamento de Erros
Erros léxicos (um caractere que não pertence à linguagem) e sintáticos (uma construção fora da gramática) são reportados com **linha e coluna**, e a compilação é interrompida antes de qualquer som:

```text
[ERROR] erro de sintaxe: Linha 1, Coluna 1: syntax error
```

Erros semânticos usam o mesmo formato. Todos são reportados de uma vez, sem cascata: um nome indefinido dentro de uma expressão gera um erro só. O arquivo `exemplos/erros_semanticos.mus` traz um caso de cada:

```text
[ERROR] erro semântico: Linha 14, Coluna 14: 'frase' usado antes de ser declarado ou fora de escopo
[ERROR] erro semântico: Linha 17, Coluna 1: redeclaração inválida: 'tempo' é numero, recebeu booleano
[ERROR] erro semântico: Linha 21, Coluna 6: operação inválida: nota + nota
```

Programas aninhados fundo demais são recusados ainda no parser, porque as fases seguintes percorrem a árvore por recursão e poderiam estourar a pilha do processo. O limite é de **256 níveis** na árvore: expressões compostas e `repita` dentro de `repita` contam, parênteses não. O erro aponta onde o limite foi cruzado:

```text
[ERROR] erro de sintaxe: Linha 1, Coluna 516: aninhamento passa do limite de 256 níveis
```

## 6. Validação
Os requisitos acima são verificados por uma suíte de testes automatizados (doctest + CTest), com casos dedicados ao lexer, ao parser, à AST, à tabela de símbolos, à análise semântica e ao interpretador. A suíte roda no CI, em Linux e macOS, a cada pull request.

