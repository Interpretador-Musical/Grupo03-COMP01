---
title: "Ata — 16/09/2026"
layout: default
parent: Documentação
nav_order: 25
---

# Ata de Reunião — 16/09/2026

- **Projeto:** Interpretador de linguagem de programação musical
- **Disciplina:** Compiladores 1 — Turma 01 (Prof. Sergio) — FCTE/UnB
- **Horário:** 14:00 – 16:00
- **Local / formato:** Sala da FCTE / Presencial
- **Ata anterior:** [09/09/2026](./2026-09-09-ata-reuniao.md)

## Participantes

- Arthur Luiz
- Caio Melo Borges
- Cecília Costa
- Julia Oliveira
- Marcella Anderle

## Pauta

1. **Decisão Crítica de Arquitetura:** Compilação em Lote (*Batch*) vs. *Live Coding* Contínuo
2. Estruturação da suíte formal de testes com `doctest` e `CTest`
3. Implantação de Integração Contínua (CI) com `ThreadSanitizer` (TSAN)
4. Modelagem da estrutura de dados intermediária (`Timeline` e `SoundEvent`)
5. Planejamento das entregas para o Ponto de Controle 1 (P1)

## Discussões e decisões

### 1. Decisão do Modelo Batch (Compilação em Lote)

O grupo debateu aprofundadamente o modelo operacional do sistema:
- Inicialmente, havia uma ambiguidade conceitual inspirada em ferramentas de *live coding* (como Sonic Pi e TidalCycles), sugerindo streaming contínuo ou hot-reloading em tempo de execução.
- **Avaliação de Risco:** Um modelo de streaming infinito prejudicaria o foco avaliativo de Compiladores 1, introduzindo complexidades excessivas de sincronismo concorrente e enfraquecendo as fases clássicas de compilação (análise completa, validação semântica e representação intermediária).
- **Decisão Arquitetural Consolidada (ADR 001):** O compilador operará formalmente em **modelo batch de programa finito**. O arquivo `.mus` de entrada é completamente analisado pelo analisador léxico, verificado sintática e semanticamente, e traduzido integralmente para uma estrutura intermediária (`Timeline`) em memória **antes** que qualquer evento chegue ao hardware de som.
- Essa decisão garante determinismo total, reprodutibilidade em testes de regressão automatizados e alinhamento estrito com o Guia de Projeto de Compiladores do professor Sergio Freitas.

### 2. Infraestrutura de Testes Automatizados (doctest + CTest)

**Decisão:** Adotar a biblioteca `doctest` (single-header C++) integrada nativamente ao `CTest`:
- Foram criados testes unitários cobrindo estruturas do core (`Arc`, `Hap`, `Pattern`, `Note`, `SoundEvent`), além de testes para a CLI (`--demo`, `--bpm`).
- Todos os testes devem rodar via comando `ctest --output-on-failure`.

### 3. Integração Contínua no GitHub Actions com ThreadSanitizer

Para assegurar estabilidade concorrente entre a thread de agendamento e a thread de callback de áudio da `miniaudio`:
- Foi configurado um workflow de CI (`.github/workflows/ci.yml`) que compila e executa toda a suíte de testes em cada Pull Request e push na branch `main`.
- Foi adicionado um job específico com **ThreadSanitizer (TSAN)** habilitado (`-fsanitize=thread`), garantindo ausência de corridas de dados (*data races*).

## Pendências / próximos passos

- [ ] Finalizar as regras gramaticais do Bison para o fechamento do P1 (CMP-02, CMP-03)
- [ ] Implementar o buffer lock-free circular SPSC para envio de eventos ao áudio (SCH-03)
- [ ] Implementar a decodificação de amostras de áudio e aplicação de envelopes (DSP-02, DSP-03)
- [ ] Elaborar a documentação formal de requisitos (DOC-02) e visão de arquitetura (DOC-01) para a entrega do P1
