---
title: "Ata — 26/08/2026"
layout: default
parent: Documentação
nav_order: 2
---

# Ata de Reunião — 26/08/2026

- **Projeto:** Interpretador de linguagem de programação musical
- **Disciplina:** Compiladores 1 — Turma 01 (Prof. Sergio) — FCTE/UnB
- **Horário:** *(a confirmar)*
- **Local / formato:** *(a confirmar)*
- **Ata anterior:** [19/08/2026](./2026-08-19-ata-reuniao.md)

## Participantes

- Arthur Luiz
- Caio Melo Borges
- Cecília Costa
- Julia Oliveira
- Marcella Anderle

## Pauta

1. Revisão da decisão sobre a implementação do lexer e do parser
2. Definição do fluxo de trabalho entre os repositórios
3. Confirmação da composição do grupo
4. Panorama dos itens ainda em aberto

## Discussões e decisões

### 1. Lexer e parser: mudança para Flex + Bison

Na concepção inicial, cogitava-se implementar o analisador sintático como um
**recursivo descendente escrito manualmente**. Essa decisão foi **revista nesta
reunião**.

**Decisão atualizada:** o lexer e o parser serão feitos com **Flex + Bison**.

**Justificativa:** essa é a abordagem que o professor demonstrou em aula, e o
material da disciplina já traz exemplos diretamente aproveitáveis — entre eles um
arquivo Flex de exemplo com regras do tipo `KW_IF` / `KW_WHILE`, identificadores,
números, operadores e comentários. Alinhar a implementação ao que foi visto em aula
reduz risco e facilita a avaliação.

O pipeline definido na reunião anterior permanece o mesmo, apenas com as duas
primeiras etapas agora nominalmente atribuídas às ferramentas:

```
código-fonte (.mus)
  → Flex (lexer)   → tokens
  → Bison (parser) → AST
  → Interpretador (tree-walking)
  → lista de SoundEvent
  → miniaudio → som
```

As demais decisões técnicas da reunião de 19/08 (C++17, CMake, miniaudio, separação
entre lógica de linguagem e lógica de áudio, conceito de *playhead*) **permanecem
válidas e não foram alteradas**.

### 2. Fluxo de trabalho entre repositórios

Foi acertado como o grupo lida com os dois repositórios envolvidos:

| Repositório | Papel |
|---|---|
| `sergioaafreitas/COMP1` (repositório do professor) | **Somente consulta / referência.** Contém material de contexto da disciplina: slides, exemplos, exercícios (incluindo o arquivo Flex de exemplo). |
| Repositório do grupo | **Repositório de desenvolvimento.** É onde vão o código, a documentação e a gramática do projeto. |

**Regras acordadas:**

- **Nenhuma alteração de desenvolvimento do projeto deve ser commitada no repositório
  do professor.**
- Todo o versionamento real do projeto acontece no repositório do grupo.
- Antes de qualquer commit ou push, **confirmar qual é o repositório alvo**.

### 3. Composição do grupo

Foi registrado explicitamente que **o trabalho é de grupo**, e não restrito a duas
pessoas. O grupo é composto por **Arthur Luiz, Caio Melo Borges, Cecília Costa,
Julia Oliveira e Marcella Anderle**. Ao documentar decisões, autoria ou divisão de
tarefas, deve-se considerar todos os integrantes.

### 4. Panorama dos itens em aberto

O grupo revisou o que ainda não foi decidido/produzido. Nenhum desses pontos foi
fechado nesta reunião — eles seguem como pendências (ver seção seguinte). Os que
foram destacados como mais urgentes, por bloquearem o restante:

- **Nome da linguagem** e **idioma da sintaxe** (português ou inglês, de forma
  consistente): bloqueiam a escrita da gramática e dos arquivos `.l` / `.y`.
- **Gramática EBNF formalizada**: é a base tanto do `parser.y` quanto da entrega
  documental.
- **Divisão de tarefas**: necessária para paralelizar lexer, parser, AST,
  interpretador e áudio entre os integrantes.

## Pendências / próximos passos

### Definições de linguagem
- [ ] Definir o **nome da linguagem**
- [ ] Decidir se a **sintaxe será em português ou em inglês**, mantendo consistência
      em toda a linguagem
- [ ] Escrever a **gramática EBNF completa e formalizada**

### Implementação
- [ ] Definir a **estrutura de pastas** do repositório de desenvolvimento
- [ ] Escrever os arquivos **`.l` (Flex)** e **`.y` (Bison)** da linguagem
- [ ] Definir as **classes/structs da AST** em C++
- [ ] Implementar o **interpretador tree-walking** que percorre a AST e gera os
      `SoundEvent`s
- [ ] Fazer a **integração com a miniaudio**

### Organização e entrega
- [ ] Fazer a **divisão de tarefas** entre os integrantes
- [ ] Produzir a **documentação**: EBNF, diagrama do pipeline, exemplos de código na
      linguagem e justificativa das decisões de design
