---
title: Roadmap
layout: default
nav_order: 4
---

# Roadmap

Estado das pendências levantadas nas [atas de reunião](https://github.com/Interpretador-Musical/Grupo03-COMP01/tree/main/docs/atas).

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

## Núcleo do Compilador

| Item | Estado | Depende de |
|---|---|---|
| Gramática EBNF completa | aberto | — (o vocabulário já está fechado no lexer) |
| `lexer.l` com os tokens da linguagem | **feito** (CMP-02) | — |
| `parser.y` reconhecendo comandos e repetição | **feito** (CMP-02) | — |
| Classes/structs da AST em C++ | **feito** (INT-03) | — |
| `parser.y` construindo a AST | **feito** (INT-03) | — |
| Compilador tree-walking com playhead (Geração de Eventos) | **feito** (INT-03) | — |
| Suporte a condicionais (`se/entao`) e funções (`defina/retorna`) | aberto | Gramática EBNF |
| Programas `.mus` de exemplo | **iniciado** — `exemplos/` | EBNF para os casos avançados |
| Erros léxicos e sintáticos com linha e coluna | **parcial** — o lexer já rastreia (CMP-03) | parser |

A cadeia base do compilador (Lexer → Parser → AST → Timeline) já está conectada de ponta a ponta. O desenvolvimento agora foca-se na expansão da linguagem (condicionais, funções) e no rigor teórico exigido pelo P1 (EBNF).

## Opcional

| Item | Estado |
|---|---|
| Trilhas paralelas (múltiplos playheads, com fusão das timelines) | fora da primeira entrega |

As atas registram esta *feature* como avançada e opcional desde a concepção. É o corte natural para manter o foco nas funcionalidades essenciais do P1 e P2.

## Organização

| Item | Estado |
|---|---|
| Divisão de tarefas entre os integrantes | **feito** (via Kanban e Issues) |
| Documentação oficial (Visão, Requisitos, Ambiente) | **feito** (DOC-01, DOC-02) |
| Documentação teórica (EBNF, Guia de Sintaxe) | aberto |
