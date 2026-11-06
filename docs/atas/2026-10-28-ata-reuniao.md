---
title: "Ata — 28/10/2026"
layout: default
parent: Documentação
nav_order: 31
---

# Ata de Reunião — 28/10/2026

- **Projeto:** Interpretador de linguagem de programação musical
- **Disciplina:** Compiladores 1 — Turma 01 (Prof. Sergio) — FCTE/UnB
- **Horário:** 14:00 – 16:30
- **Local / formato:** Sala da FCTE / Presencial
- **Ata anterior:** [21/10/2026](./2026-10-21-ata-reuniao.md)

## Participantes

- Arthur Luiz
- Caio Melo Borges
- Cecília Costa
- Julia Patricio
- Marcella Anderle

## Pauta

1. Homologação do lançamento da **Release M1 (INT-04)**
2. Validação dos merges críticos da Sprint 04:
   - Análise Semântica e Tabela de Símbolos (CMP-13)
   - Limite de profundidade de 256 níveis na AST (CMP-08)
   - Auditoria de memória com AddressSanitizer na CI (MAT-11)
   - Limitador suave no master bus de áudio (DSP-10)
3. Apresentação da suíte de integração e emissão de IR em CSV (`--emit-timeline`)
4. Revisão da minuta do **Relatório de Verificação do P2 (DOC-04)**
5. Decisão de inclusão de Trilhas Paralelas (CMP-07) no escopo final do P2

## Discussões e decisões

### 1. Homologação da Release M1 (INT-04)

A equipe comemorou a conclusão bem sucedida da Release M1:
- A release consolida o compilador batch funcional com build automatizado em CMake, suporte a Linux, WSL e macOS, e suíte de testes de regressão automatizada.
- As notas de release foram documentadas na página [Release M1](../release_m1.md) do site do projeto.

### 2. Validação da Análise Semântica (CMP-13) e Qualidade

Foram revisados os PRs aprovados nesta semana:
- **CMP-13:** Análise semântica integrada e validada com suíte de testes unitários (`test_tabela_simbolos.cpp`, `test_analisador_semantico.cpp`).
- **MAT-11:** Inclusão do job com **AddressSanitizer (ASAN)** na CI (`-fsanitize=address,undefined -fno-omit-frame-pointer`). A execução automatizada atestou a ausência de vazamentos de memória na suíte de testes cobrindo parsing e destruição de nós da AST via `%destructor`.
- **CMP-08:** O parser agora impõe limite rígido de 256 níveis de aninhamento, prevenindo estouro de pilha por recursão anômala.
- **DSP-10:** Foi integrado o limitador no master bus de saída, protegendo o sinal de áudio contra *clipping* acústico.

### 3. Emissão Determinística da Timeline (COMP-B)

Foi validada a flag `--emit-timeline <arquivo.mus>`:
- Ela gera uma saída CSV com os campos `start_time,duration,frequency,volume`.
- A saída determinística permitiu criar testes automatizados no CTest comparando a saída byte a byte com gabaritos canônicos esperados.

### 4. Decisão sobre Trilhas Paralelas (CMP-07)

Discutiu-se a possibilidade de incorporar a sintaxe de trilhas paralelas `{ trilha1, trilha2 }` antes da apresentação do P2:
- **Decisão:** Como a representação intermediária baseada em `Timeline` permite agendar múltiplos eventos com início coincidente, a equipe decidiu implementar a feature CMP-07 nesta reta final, agregando polirritmia e riqueza musical ao projeto para a banca do P2.

## Pendências / próximos passos

- [ ] Integrar a implementação e testes das Trilhas Paralelas (CMP-07)
- [ ] Concluir e revisar o Relatório de Verificação do P2 (DOC-04 / relatorio_p2.md)
- [ ] Rodar suíte de testes de estresse com 64 vozes sobrepostas (INT-07)
- [ ] Revisar e alinhar a documentação geral, README e site para a apresentação do P2
