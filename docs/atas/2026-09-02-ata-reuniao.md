---
title: "Ata — 02/09/2026"
layout: default
parent: Documentação
nav_order: 23
---

# Ata de Reunião — 02/09/2026

- **Projeto:** Interpretador de linguagem de programação musical
- **Disciplina:** Compiladores 1 — Turma 01 (Prof. Sergio) — FCTE/UnB
- **Horário:** 14:00 – 15:30
- **Local / formato:** Sala da FCTE / Presencial
- **Ata anterior:** [26/08/2026](./2026-08-26-ata-reuniao.md)

## Participantes

- Arthur Luiz
- Caio Melo Borges
- Cecília Costa
- Julia Oliveira
- Marcella Anderle

## Pauta

1. Fechamento formal da Sprint 01 e abertura da Sprint 02
2. Estruturação do repositório no GitHub e publicação do site (GitHub Pages)
3. Configuração do ambiente de compilação CMake com suporte a Flex e Bison
4. Definição do prefixo das issues e atribuição de responsabilidades iniciais
5. Alinhamento da sintaxe em português da linguagem

## Discussões e decisões

### 1. Fechamento da Sprint 01 e início da Sprint 02

O grupo revisou as metas iniciais da Sprint 01. O repositório oficial da equipe foi criado no GitHub (`Interpretador-Musical/Grupo03-COMP01`), desvinculando o desenvolvimento do repositório de exemplos da disciplina. O planejamento passa a seguir o cronograma semestral formal.

### 2. Infraestrutura de documentação: GitHub Pages

**Decisão:** a documentação pública do projeto será publicada via **GitHub Pages** utilizando o tema `just-the-docs`.
- Toda decisão técnica e ata de reunião deve ser versionada em Markdown na pasta `docs/`.
- O site servirá de vitrine e rastreabilidade para o professor e monitores durante as avaliações.

### 3. Integração do CMake com Flex e Bison

Discutiu-se a integração dos geradores automáticos de código com o CMake:
- O CMake utilizará os módulos `find_package(FLEX REQUIRED)` e `find_package(BISON REQUIRED)`.
- Os arquivos fontes gerados (`lexer.cpp` e `parser.cpp`) serão mantidos fora do controle de versão git, gerados diretamente dentro do diretório `build/`.
- Ficou definido o suporte primário a Linux e Windows (via WSL), com dependências documentadas.

### 4. Sistema de rotatividade e prefixos de issues

Para organizar o trabalho em equipe e atender à exigência de rotatividade de papéis e rastreabilidade da disciplina, definiram-se os prefixos de issues:
- `CMP-` (Compilador / Front-end: Léxico, Sintático, AST)
- `MAT-` (Core matemático: Playhead, durações, frequências, estruturas)
- `SCH-` (Scheduler / Engine: Loop temporal, sincronismo)
- `DSP-` (Processamento de Sinal / Áudio: miniaudio, envelopes, buffers)
- `INT-` (Integração e CLI: Entrypoint, testes ponta a ponta)
- `DOC-` (Documentação e Gestão: EBNF, especificações, atas, relatórios)

As primeiras issues de implementação foram abertas e distribuídas entre os cinco integrantes.

### 5. Sintaxe em português

Confirmou-se que as palavras-chave serão em português (ex: `toca`, `pausa`, `andamento`, `repita ... vezes`), com o separador obrigatório `por` para evitar ambiguidades com operadores matemáticos.

## Pendências / próximos passos

- [ ] Implementar a inicialização do dispositivo miniaudio e emissão de tom senoidal (DSP-01)
- [ ] Implementar as estruturas básicas de notas e eventos temporais no core (MAT-01)
- [ ] Estruturar a CLI com opções iniciais e logger (INT-01)
- [ ] Implementar o loop principal de tempo com relógio monotônico (SCH-01, SCH-02)
- [ ] Configurar a primeira versão do analisador léxico com Flex (CMP-01)
