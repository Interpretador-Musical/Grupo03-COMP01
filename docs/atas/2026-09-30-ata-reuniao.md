---
title: "Ata — 30/09/2026"
layout: default
parent: Documentação
nav_order: 27
---

# Ata de Reunião — 30/09/2026

- **Projeto:** Interpretador de linguagem de programação musical
- **Disciplina:** Compiladores 1 — Turma 01 (Prof. Sergio) — FCTE/UnB
- **Horário:** 14:00 – 15:30
- **Local / formato:** Sala da FCTE / Presencial
- **Ata anterior:** [23/09/2026](./2026-09-23-ata-reuniao.md)

## Participantes

- Arthur Luiz
- Caio Melo Borges
- Cecília Costa
- Julia Oliveira
- Marcella Anderle

## Pauta

1. Retrospectiva do Ponto de Controle 1 (P1) e feedback recebido
2. Planejamento das metas da Sprint 03 (24/09 a 14/10)
3. Atualização do README e do site com a nova arquitetura batch
4. Discussão sobre a necessidade de Análise Semântica e Tabela de Símbolos
5. Rotatividade de papéis para a Sprint 03

## Discussões e decisões

### 1. Retrospectiva do P1

O grupo avaliou que a entrega do P1 foi muito bem sucedida. O pipeline comprovou que a linguagem musical é viável e já produz sons reais a partir do código `.mus`. No entanto, foram levantadas oportunidades de melhoria para os próximos pontos de controle:
- A AST inicial era simplificada e não possuía tipos heterogêneos bem estruturados em C++;
- O interpretador acessava nós diretamente sem uma fase intermediária explícita de verificação semântica;
- A documentação de instruções de build precisava de ajustes para cobrir pacotes de áudio (`libasound2-dev`, `libpulse-dev`).

### 2. Atualização documental (README e Site)

Foram integrados pull requests atualizando o `README.md`, o guia de ambiente e o menu de navegação do site, refletindo a arquitetura em lote e corrigindo orientações de clone e flags da CLI.

### 3. Planejamento da Sprint 03

**Metas estabelecidas para a Sprint 03:**
1. **Modelagem robusta da AST:** Definir nós em C++17 com segurança de tipos, controle estrito de ciclo de vida de memória e proteção contra aninhamento excessivo.
2. **Concepção da Análise Semântica:** Iniciar o desenho da Tabela de Símbolos para gerenciar variáveis, escopos locais e tipos (`numero`, `nota`, `booleano`).
3. **Cálculos e divisões temporais:** Implementação de divisão recursiva de arcos temporais (MAT-05) para enriquecer a expressividade métrica da linguagem.

### 4. Rotatividade de papéis

Seguindo a política ágil da equipe e as orientações da disciplina, foram rotacionadas as responsabilidades entre os integrantes nas frentes de compilador (frontend), análise semântica, infraestrutura de testes e documentação técnica.

## Pendências / próximos passos

- [ ] Elaborar a proposta de arquitetura da AST tipada com C++17 (DOC-03 / CMP-04)
- [ ] Implementar a divisão recursiva de arcos temporais (MAT-05)
- [ ] Investigar requisitos de gerenciamento seguro de memória para o Bison (%destructor)
- [ ] Realizar benchmarks de precisão temporal e mitigar riscos de underrun (DSP-04)
