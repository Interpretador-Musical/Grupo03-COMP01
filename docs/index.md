---
title: Sobre o projeto
layout: default
nav_order: 1
permalink: /
---

# Interpretador Musical
{: .fs-9 .mb-2 }

Uma linguagem de programação que vira música.
{: .fs-6 .fw-300 .text-grey-dk-000 }

[Documentação]({{ site.baseurl }}{% link documentacao.md %}){: .btn .btn-primary .mr-2 }
[Roadmap]({{ site.baseurl }}{% link roadmap.md %}){: .btn }

---

## O que é

Este projeto é um **interpretador que traduz uma linguagem de programação própria em
som**. Em vez de imprimir texto na tela, um programa escrito nessa linguagem produz
música — o conceito é próximo ao de ferramentas como o Sonic Pi, mas aqui o
lexer, o parser e o interpretador são inteiramente autorais.

É o trabalho do Grupo 03 na disciplina de **Compiladores 1** (Turma 01, Prof. Sérgio),
na FCTE/UnB.

## Por que não é só um tradutor de notas

Ao aprovar a ideia, o professor colocou uma condição: a linguagem precisa ter
complexidade real para valer como trabalho de Compiladores. Não basta um tradutor
linear do tipo "nota X toca por tempo Y".

Por isso a linguagem precisa suportar:

- variáveis (tempo, tom, andamento);
- estruturas de repetição;
- condicionais;
- funções com parâmetros;
- expressões aritméticas, para transposição de notas e cálculo de durações;
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

Uma decisão de arquitetura atravessa todo o projeto: **o interpretador não toca som**.
Ele apenas produz uma lista de eventos temporizados —

```cpp
struct SoundEvent {
    float startTime;   // quando começa, em segundos
    float duration;    // quanto dura
    float frequency;   // altura da nota, em Hz
    float volume;      // amplitude
};
```

— que só depois é entregue ao motor de áudio. Isso mantém a lógica de linguagem
(lexer, parser, interpretador) separada da lógica de áudio, e é o que torna o
interpretador testável sem depender de placa de som.

## O playhead

O que diferencia este interpretador de um interpretador comum é o **playhead**: um
cursor de tempo que avança conforme o programa executa. Tocar uma nota não emite som
na hora — agenda um evento na posição atual do cursor e empurra o cursor para frente.

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

{: .aviso }
> A sintaxe abaixo é **ilustrativa**. O nome da linguagem e o idioma das palavras-chave
> ainda não foram decididos — é justamente o primeiro item do
> [roadmap]({{ site.baseurl }}{% link roadmap.md %}).

```text
tempo = 0.5

repita 4 vezes {
    toca do  tempo
    toca mi  tempo
    toca sol tempo
}
```

Quatro repetições de um arpejo: o mesmo trecho de código, executado quatro vezes,
vira quatro compassos — porque o playhead avança a cada nota.

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

O projeto está na fase de definição da linguagem. As decisões tomadas até aqui estão
registradas nas [atas de reunião]({{ site.baseurl }}{% link documentacao.md %}), e o
que falta está no [roadmap]({{ site.baseurl }}{% link roadmap.md %}).
