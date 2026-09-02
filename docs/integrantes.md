---
title: Integrantes
layout: default
nav_order: 3
---

# Integrantes

O Grupo 03 é formado por cinco estudantes da FCTE/UnB.

| Integrante | GitHub | Frente de trabalho |
|---|---|---|
| Arthur Luiz | — | a definir |
| Caio Melo Borges | [@CaioMelo25](https://github.com/CaioMelo25) | a definir |
| Cecília Costa | — | a definir |
| Julia Oliveira | — | a definir |
| Marcella Anderle | — | a definir |

{: .nota }
> Os usuários do GitHub e a frente de trabalho de cada integrante ainda não foram
> preenchidos: a divisão de tarefas é uma das pendências abertas na
> [ata de 26/08]({{ site.baseurl }}{% link atas/2026-08-26-ata-reuniao.md %}).

## Como o trabalho se divide

O projeto se separa naturalmente em cinco frentes, que correspondem às etapas do
pipeline mais a infraestrutura:

1. **Infraestrutura** — estrutura de pastas, build CMake, integração Flex/Bison.
2. **Análise léxica** — o arquivo `.l`, os tokens e os erros léxicos.
3. **Gramática e AST** — a EBNF formal e as classes que representam o programa.
4. **Análise sintática** — o arquivo `.y` e a construção da AST.
5. **Interpretador e áudio** — o playhead, a geração de `SoundEvent` e a miniaudio.

A atribuição de cada frente a cada pessoa será registrada aqui assim que for decidida.
