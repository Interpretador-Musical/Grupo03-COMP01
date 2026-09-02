---
title: Roadmap
layout: default
nav_order: 4
---

# Roadmap

Estado das pendências levantadas nas [atas de reunião]({{ site.baseurl }}{% link documentacao.md %}).

## Desbloqueio

Estes três podem andar em paralelo — os dois últimos não dependem de nenhuma decisão de
linguagem.

| Item | Estado | Trava o quê |
|---|---|---|
| Definir o nome da linguagem e o idioma da sintaxe | aberto | gramática, lexer e todos os exemplos |
| Prova de conceito com miniaudio a partir de `SoundEvent` | aberto | congela o formato do struct |
| Estrutura de pastas e build CMake com Flex/Bison | aberto | toda a implementação |

{: .nota }
> A primeira linha é uma decisão de reunião, não uma tarefa de sprint — mas é o que mais
> gente está esperando para começar a trabalhar.

## Núcleo do compilador

| Item | Estado | Depende de |
|---|---|---|
| Gramática EBNF completa | aberto | nome e idioma da sintaxe |
| `lexer.l` com os tokens da linguagem | aberto | nome e idioma; build |
| Classes/structs da AST em C++ | aberto | EBNF |
| `parser.y` construindo a AST | aberto | EBNF, lexer, AST |
| Interpretador tree-walking com playhead | aberto | AST, prova de conceito de áudio |
| Programas `.mus` de exemplo | aberto | nome e idioma; EBNF |
| Erros léxicos e sintáticos com linha e coluna | aberto | lexer, parser |

A cadeia EBNF → AST → parser é serial e é o trecho mais provável de apertar o
cronograma.

## Opcional

| Item | Estado |
|---|---|
| Trilhas paralelas (múltiplos playheads, com fusão das timelines) | fora da primeira entrega |

As atas registram esta feature como avançada e opcional desde a concepção. É o corte
natural se o prazo apertar.

## Organização

| Item | Estado |
|---|---|
| Divisão de tarefas entre os integrantes | aberto |
| Documentação de entrega (EBNF, diagrama do pipeline, exemplos, decisões justificadas) | em andamento |
