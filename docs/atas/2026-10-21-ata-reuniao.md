---
title: "Ata — 21/10/2026"
layout: default
parent: Documentação
nav_order: 30
---

# Ata de Reunião — 21/10/2026

- **Projeto:** Interpretador de linguagem de programação musical
- **Disciplina:** Compiladores 1 — Turma 01 (Prof. Sergio) — FCTE/UnB
- **Horário:** 14:00 – 15:30
- **Local / formato:** Sala da FCTE / Presencial
- **Ata anterior:** [14/10/2026](./2026-10-14-ata-reuniao.md)

## Participantes

- Arthur Luiz
- Caio Melo Borges
- Cecília Costa
- Julia Patricio
- Marcella Anderle

## Pauta

1. Acompanhamento do desenvolvimento da Tabela de Símbolos e Análise Semântica (CMP-13)
2. Definição do contrato de saída e formato da flag `--simbolos`
3. Preparação dos pacotes e evidências para a **Release M1 (INT-04)**
4. Formalização das 6 invariantes da Representação Intermediária (`Timeline`) (COMP-A)
5. Alinhamento sobre checagem de tipos estáticos na linguagem

## Discussões e decisões

### 1. Implementação da Tabela de Símbolos (CMP-13)

Foi apresentado o protótipo funcional do analisador semântico:
- A `TabelaSimbolos` armazena o mapa de variáveis contendo: nome, tipo inferido na primeira atribuição, nível de escopo lexico, endereço (*slot* de memória) e registros das linhas e colunas de declaração e usos.
- **Regras Semânticas Estabelecidas:**
  1. A primeira atribuição infere o tipo estático da variável.
  2. Tentativa de reatribuição com tipo distinto acusa erro de redeclaração inválida (ex: `tempo = 0.5` seguido de `tempo = verdadeiro`).
  3. Tentativa de ler variável não declarada acusa erro semântico precoce.
  4. Falhas semânticas encerram o compilador com código de erro 1, sem executar a AST ou gerar eventos sonoros.

### 2. Flag `--simbolos` na CLI

**Decisão:** Expor a tabela de símbolos publicamente através da opção `--simbolos <arquivo.mus>`:
- O compilador faz a análise léxica, sintática e semântica, imprime a tabela com todos os identificadores, seus tipos e escopos, e finaliza a execução com sucesso sem gerar áudio.
- Essa flag fornece evidência direta e inspecionável para a banca do P2.

### 3. Representação Intermediária (Timeline) e as 6 Invariantes (COMP-A)

Ficou acordado que a `Timeline` é o código intermediário (equivalente a uma sequência de quádruplas) do compilador:
- Cada `SoundEvent` é composto por `(startTime, duration, frequency, volume)`.
- Foram formalizadas 6 invariantes: início não-negativo, duração estritamente positiva, frequência positiva, volume no intervalo $[0.0, 1.0]$, ordenação cronológica monotonicamente crescente por `startTime` e determinismo numérico.

### 4. Preparação da Release M1

Ficaram definidos os requisitos da primeira release formal (Release M1 - `release/int-04-release-m1`), marcando a primeira versão estável do compilador para demonstração aos monitores e professor.

## Pendências / próximos passos

- [ ] Lançar formalmente a Release M1 com testes de empacotamento (INT-04)
- [ ] Integrar a PR de Análise Semântica (CMP-13)
- [ ] Implementar a flag `--emit-timeline` com saída determinística em CSV (COMP-B)
- [ ] Implementar o limitador suave no master bus para mitigar distorções (DSP-10)
- [ ] Concluir o relatório de verificação e evidências do P2 (DOC-04)
