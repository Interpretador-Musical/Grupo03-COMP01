---
title: "Ata — 14/10/2026"
layout: default
parent: Documentação
nav_order: 29
---

# Ata de Reunião — 14/10/2026

- **Projeto:** Interpretador de linguagem de programação musical
- **Disciplina:** Compiladores 1 — Turma 01 (Prof. Sergio) — FCTE/UnB
- **Horário:** 14:00 – 16:00
- **Local / formato:** Sala da FCTE / Presencial
- **Ata anterior:** [07/10/2026](./2026-10-07-ata-reuniao.md)

## Participantes

- Arthur Luiz
- Caio Melo Borges
- Cecília Costa
- Julia Patricio
- Marcella Anderle

## Pauta

1. Encerramento no calendário da Sprint 03 e avaliação do backlog
2. Abertura e planejamento estratégico da **Sprint 04 (reta para o P2)**
3. Reclassificação de escopo: foco estrito nas exigências formais de compilador
4. Definição do plano de entregas para o Ponto de Controle 2 (P2)
5. Rotatividade de papéis para a Sprint 04

## Discussões e decisões

### 1. Avaliação da Sprint 03 e Replanejamento para a Sprint 04

O grupo analisou o estado das tarefas ao término da Sprint 03:
- Foram concluídas as bases estruturais da AST (DOC-03) e divisão recursiva de arcos (MAT-05).
- Algumas issues de semântica e infraestrutura ficaram pendentes e foram carregadas para a Sprint 04.
- **Decisão:** A Sprint 04 será o período decisivo para consolidar os requisitos do **Ponto de Controle 2 (P2)**. O foco deve ser 100% voltado aos estágios clássicos de compiladores.

### 2. Priorização do P2 e Gestão de Escopo

Para garantir a excelência na avaliação do P2 e evitar dispersão com funcionalidades acessórias:
- **Itens Críticos (P0/P1 do P2):**
  1. **Análise Semântica e Tabela de Símbolos (CMP-13):** Verificação de tipos (`numero`, `nota`, `booleano`), checagem de variáveis não declaradas e escopos léxicos.
  2. **Representação Intermediária Formal (COMP-A e COMP-B):** Formalização da `Timeline` como código intermediário canônico e criação da flag `--emit-timeline` com saída determinística para auditoria.
  3. **Gramática EBNF Formalizada (DOC-A):** Especificação completa conferida diretamente contra as produções do `parser.y`.
  4. **Auditoria de Qualidade e Memória (MAT-11 e INT-06):** Pipeline de CI com AddressSanitizer (ASAN) e suíte de testes de integração ponta a ponta no CTest.
  5. **Limitador de Áudio Master (DSP-10) e Limite de Profundidade (CMP-08).**
- **Itens Adiados / Fora do P2:** Dot-chaining (`CMP-06`), estruturas condicionais complexas (`se/entao/senao`) e closures em tempo de execução (`MAT-06`) ficam fora do caminho crítico do P2, sendo registradas como decisões defensáveis de engenharia.

### 3. Rotatividade de papéis

Para a Sprint 04, a liderança técnica e as responsabilidades foram rotacionadas:
- Semântica e Front-end: Arthur Luiz e Caio Melo Borges
- Core Matemático e Qualidade/ASAN: Julia Patricio e Marcella Anderle
- Integração, CLI e Documentação/Relatório P2: Cecília Costa

## Pendências / próximos passos

- [ ] Finalizar o desenvolvimento e testes da Tabela de Símbolos (CMP-13)
- [ ] Homologar a Release M1 (INT-04)
- [ ] Implementar a flag `--emit-timeline` e validar invariantes da IR (COMP-A, COMP-B)
- [ ] Configurar job de AddressSanitizer no GitHub Actions (MAT-11)
- [ ] Redigir a Gramática EBNF e o ADR da decisão batch (DOC-A, DOC-B)
