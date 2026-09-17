---
title: Roadmap
layout: default
nav_order: 4
---

# Roadmap

Estado das pendências levantadas nas [atas de reunião]({{ site.baseurl }}{% link documentacao.md %}).

## Desbloqueio

Os itens que travavam o resto do projeto.

| Item | Estado | Trava o quê |
|---|---|---|
| Definir o idioma da sintaxe | **fechado** — português, notas `do re mi` | — |
| Definir o nome da linguagem | aberto | só documentação; nenhum arquivo de código depende dele |
| Prova de conceito com miniaudio a partir de `SoundEvent` | **feito** (DSP-01) | — |
| Estrutura de pastas e build CMake com Flex/Bison | **feito** (CMP-01) | — |

{: .nota }
> O nome da linguagem é decisão de reunião, não tarefa de sprint. Diferente do idioma,
> ele não bloqueia ninguém: não aparece em nenhum `.l`, `.y`, `.cpp` ou `.h`.

## Núcleo do compilador

| Item | Estado | Depende de |
|---|---|---|
| Gramática EBNF completa | aberto | — (o vocabulário já está fechado no lexer) |
| `lexer.l` com os tokens da linguagem | **feito** (CMP-02) | — |
| Classes/structs da AST em C++ | aberto | EBNF |
| `parser.y` reconhecendo comandos e repetição | **feito** (CMP-02) | — |
| `parser.y` construindo a AST | aberto | EBNF, lexer, AST |
| Interpretador tree-walking com playhead | aberto | AST, prova de conceito de áudio |
| Programas `.mus` de exemplo | **iniciado** — `exemplos/` | EBNF para os casos avançados |
| Erros léxicos e sintáticos com linha e coluna | **parcial** — o lexer já rastreia; falta formatar o `yyerror` (CMP-03) | parser |

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
