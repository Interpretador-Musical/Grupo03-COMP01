---
title: Sobre o projeto
layout: default
nav_order: 1
permalink: /
---

# Compilador Musical
{: .fs-9 .mb-2 }

Uma Domain-Specific Language (DSL) que transforma código em música.
{: .fs-6 .fw-300 .text-grey-dk-000 }

[Documentação]({{ site.baseurl }}{% link documentacao.md %}){: .btn .btn-primary .mr-2 }
[Roadmap]({{ site.baseurl }}{% link roadmap.md %}){: .btn }

---

## O que é

Este projeto é um **compilador em lote (*batch*) que traduz uma linguagem de programação própria em som**. Em vez de imprimir texto na tela, um programa escrito nesta linguagem produz música. Todo o pipeline — desde o analisador léxico até o gerador da árvore sintática — é inteiramente autoral.

É o trabalho do Grupo 03 na disciplina de **Compiladores 1** (Turma 01, Prof. Sérgio), na FCTE/UnB.

## Por que não é só um tradutor de notas

Ao aprovar a ideia, o professor colocou uma condição: a linguagem precisa ter complexidade real para valer como trabalho de Compiladores. Não basta um tradutor linear do tipo "nota X toca por tempo Y".

Por isso a linguagem suporta:

- variáveis (tempo, tom, andamento);
- estruturas de repetição;
- condicionais (em desenvolvimento);
- funções com parâmetros (em desenvolvimento);
- expressões aritméticas e lógicas, para transposição de notas e cálculo de durações;
- comentários de linha e de bloco;
- erros léxicos e sintáticos com indicação de linha e coluna.

## Como funciona

```mermaid
flowchart LR
  A["código-fonte<br/>(.mus)"] --> B["Flex<br/>análise léxica"]
  B -- tokens --> C["Bison<br/>análise sintática"]
  C -- AST --> D["Interpretador<br/>tree-walking"]
  D -- "lista de SoundEvent" --> E["miniaudio<br/>síntese de onda"]
  E --> F(("som"))
```

Uma decisão de arquitetura atravessa todo o projeto: **o núcleo do compilador não emite som**. Ele apenas produz uma lista estática de eventos temporizados —

```cpp
struct SoundEvent {
    double startTime;  // quando começa, em segundos
    double duration;   // quanto dura, em segundos
    double frequency;  // altura da nota, em Hz
    float  volume;     // amplitude, de 0.0 a 1.0
};
```

— que só depois da compilação completa é entregue ao motor de áudio. Isso mantém a lógica de linguagem separada da lógica de processamento de sinais, tornando o compilador testável sem depender de hardware de som.

## O playhead

O conceito central deste compilador é o **playhead**: um cursor de tempo que avança conforme o interpretador percorre a árvore sintática. Agendar uma nota não bloqueia a execução — apenas insere um evento na posição atual do cursor e o empurra para a frente.

É esse mecanismo que dá sentido musical às estruturas de controle:

| Construção | Efeito musical |
|---|---|
| Sequência de comandos | Notas em sequência, uma após a outra |
| Repetição | O cursor avança a cada volta, transformando laço em **padrão rítmico** |
| Condicional | Variação: caminhos diferentes produzem trechos diferentes |
| Função | Um **motivo reutilizável**; o tempo gasto dentro dela se acumula no cursor externo |

Múltiplas vozes simultâneas exigiriam vários cursores independentes, com fusão das
timelines antes do envio ao áudio. Está registrado como feature avançada e opcional.

## Um exemplo

{: .nota }
> A sintaxe abaixo é a **definida e implementada** no lexer e no parser. O que ainda
> falta decidir é só o **nome da linguagem**, que não aparece em nenhum arquivo de
> código — ver o [roadmap]({{ site.baseurl }}{% link roadmap.md %}).

```text
andamento 120
tempo = 0.5

repita 4 vezes {
    toca do4  por tempo
    toca mi4  por tempo
    toca sol4 por tempo
}

toca la4 - 2 por tempo * 2   /* transposição e duração calculadas */
pausa por 1.0
```

Quatro repetições de um arpejo: o mesmo trecho de código, executado quatro vezes, transforma-se em quatro compassos sonoros (pois o playhead avança a cada nota processada).

Para visualizar o analisador léxico extraindo os tokens deste programa:

```bash
./build/compilador --tokens exemplos/arpejo.mus
```

### As decisões de sintaxe

| Assunto | Decisão |
|---|---|
| Idioma | Português, de ponta a ponta |
| Notas | `do re mi fa sol la si`, com `#` para sustenido e `b` para bemol |
| Oitava | Colada na nota: `do4` é o dó central (MIDI 60), `la4` é o lá de 440 Hz |
| Sem oitava | `toca do` usa a oitava corrente, definida por `oitava 4` |
| Duração | Em **tempos**: `1.0` é uma semínima, `0.5` uma colcheia. O BPM converte para segundos |
| Separador | A palavra **`por`** separa altura de duração: `toca do4 por tempo` |
| Comentários | `//` até o fim da linha e `/* */` em bloco |
| Blocos | Delimitados por `{ }` |

A palavra-chave `por` é obrigatória. Sem ela, a instrução `toca la4 - 2 tempo` seria ambígua: o parser não teria como saber se `- 2` pertence à altura ou é o começo da duração. Com o separador, os dois lados aceitam expressão à vontade e a gramática fica sem nenhum conflito de shift/reduce.

## Stack

| Camada | Escolha |
|---|---|
| Implementação | C++17 |
| Análise léxica | Flex |
| Análise sintática | Bison |
| Áudio | miniaudio (single-header, sintetiza a forma de onda) |
| Build | CMake |
| Plataforma alvo | Windows |

A engine Godot chegou a ser considerada, mas foi descartada como abordagem principal:
usar GDScript significaria depender do parser e do interpretador prontos da engine,
enfraquecendo exatamente a parte que a disciplina avalia.

## Estado atual

O compilador já opera de ponta a ponta (End-to-End). O front-end (`lexer.l` e `parser.y`) reconhece o vocabulário, as variáveis e as estruturas de repetição, gerando a Árvore Sintática Abstrata (AST) livre de conflitos. O interpretador percorre a AST convertendo o código numa timeline na memória, que por fim é injetada e reproduzida com sucesso pela engine de áudio (`miniaudio`).

O próximo passo é a expansão da linguagem (condicionais e funções). As decisões tomadas até aqui estão
registradas nas [atas de reunião]({{ site.baseurl }}{% link documentacao.md %}), e o
que falta está no [roadmap]({{ site.baseurl }}{% link roadmap.md %}).
