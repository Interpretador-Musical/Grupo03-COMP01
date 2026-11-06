---
title: "Ata — 23/09/2026"
layout: default
parent: Documentação
nav_order: 26
---

# Ata de Reunião — 23/09/2026

- **Projeto:** Interpretador de linguagem de programação musical
- **Disciplina:** Compiladores 1 — Turma 01 (Prof. Sergio) — FCTE/UnB
- **Horário:** 14:00 – 16:30
- **Local / formato:** Sala da FCTE / Presencial
- **Ata anterior:** [16/09/2026](./2026-09-16-ata-reuniao.md)

## Participantes

- Arthur Luiz
- Caio Melo Borges
- Cecília Costa
- Julia Patricio
- Marcella Anderle

## Pauta

1. Consolidação e entrega do **Ponto de Controle 1 (P1)**
2. Validação da integração ponta a ponta: do código `.mus` até o som (INT-03)
3. Apresentação do scanner e parser inicial com Flex e Bison (CMP-02, CMP-03)
4. Comunicação segura com ring buffer lock-free SPSC (SCH-03)
5. Homologação da Especificação de Requisitos (DOC-02) e Guia de Ambiente (DOC-01)
6. Fechamento formal da Sprint 02

## Discussões e decisões

### 1. Entrega do Ponto de Controle 1 (P1)

O grupo reuniu todos os artefatos exigidos pelo professor para a avaliação do **P1 (Sprint 02)**. O compilador demonstrou a capacidade de:
- Analisar um arquivo de exemplo `.mus` (`exemplos/arpejo.mus`);
- Reconhecer notas musicais, acidentes, oitavas e o comando `repita N vezes { ... }`;
- Reportar erros léxicos e sintáticos com indicação precisa de linha e coluna;
- Sintetizar o áudio correspondente via miniaudio.

### 2. Fluxo ponta a ponta conectado (INT-03)

O pipeline foi formalmente conectado:
- O módulo `frontend` executa `mus_lex` e `mus_parse`, construindo a árvore sintática em memória;
- O `interpretador` percorre a árvore gerando os `SoundEvents`;
- O comando executável `build/compilador exemplos/arpejo.mus` funciona de ponta a ponta.
- Foi implementada a flag `--no-audio` para testes determinísticos sem necessidade de hardware de som físico.

### 3. Buffer Lock-Free SPSC (SCH-03) e Envelopes (DSP-03)

- Para garantir comunicação segura entre a thread de interpretação e a thread de áudio em tempo real sem deadlocks, foi implementado um **Ring Buffer SPSC (Single Producer, Single Consumer)** atômico e *lock-free*.
- A síntese de notas passou a incluir um envelope sonoro com tempos de ataque (*attack*) e liberação (*release*) de 5 ms, eliminando cliques acústicos no início e fim de cada nota gerada.

### 4. Publicação da Documentação do P1

- O site no GitHub Pages foi atualizado com a **Visão de Arquitetura (DOC-01)**, o **Guia de Ambiente validado no Windows via WSL (DOC-01)** e a **Especificação de Requisitos (DOC-02)** com os requisitos funcionais (RF) e não funcionais (RNF).

## Pendências / próximos passos

- [ ] Planejar os objetivos da Sprint 03
- [ ] Atualizar o README com a nova estrutura e instruções de compilação
- [ ] Iniciar a estruturação da Árvore Sintática Abstrata (AST) tipada e robusta
- [ ] Mapear as necessidades de Análise Semântica (tabela de símbolos e verificação de tipos)
