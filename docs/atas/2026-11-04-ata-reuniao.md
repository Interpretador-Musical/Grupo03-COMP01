---
title: "Ata — 04/11/2026"
layout: default
parent: Documentação
nav_order: 32
---

# Ata de Reunião — 04/11/2026

- **Projeto:** Interpretador de linguagem de programação musical
- **Disciplina:** Compiladores 1 — Turma 01 (Prof. Sergio) — FCTE/UnB
- **Horário:** 14:00 – 16:00
- **Local / formato:** Sala da FCTE / Presencial
- **Ata anterior:** [28/10/2026](./2026-10-28-ata-reuniao.md)

## Participantes

- Arthur Luiz
- Caio Melo Borges
- Cecília Costa
- Julia Oliveira
- Marcella Anderle

## Pauta

1. Fechamento formal da **Sprint 04** e balanço das metas alcançadas
2. Homologação das **Trilhas Paralelas Finitas (CMP-07 / ADR 002)**
3. Validação final da suíte de integração ponta a ponta (INT-06) e estresse (INT-07)
4. Aprovação final do **Relatório de Verificação e Evidências do P2 (DOC-04)**
5. Auditoria da documentação: governança, atas completas e refatoração do README
6. Alinhamento das respostas e envio do **Formulário de Avaliação do P2**

## Discussões e decisões

### 1. Fechamento da Sprint 04 e Sucesso nas Metas do P2

A equipe realizou o balanço geral da Sprint 04, constatando que **100% dos critérios avaliativos estipulados para o P2 foram atendidos com êxito**:
- Front-end completo: Flex (`lexer.l`), Bison (`parser.y`) e Gramática EBNF correspondente;
- AST robusta em C++17 com segurança de tipos, controle estrito de ciclo de vida e limite de profundidade de 256 níveis;
- Análise Semântica completa com Tabela de Símbolos, resolução de escopos e checagem estática de tipos, acessível via `--simbolos`;
- Representação Intermediária (`Timeline`) formalizada com verificação das 6 invariantes e exportação determinística em CSV via `--emit-timeline`;
- Pipeline de testes automatizados com 22 jobs no CTest, execução sob ASAN (zero vazamentos) e TSAN (zero data races).

### 2. Homologação das Trilhas Paralelas (CMP-07 / ADR 002)

Foi aprovada e integrada a funcionalidade de trilhas paralelas `{ trilha1, trilha2, ... }`:
- Permite acordes, polirritmias e vozes simultâneas com cursores de tempo independentes;
- As regras de semântica (isolamento de variáveis e proibição de alteração global de andamento dentro de blocos paralelos) foram formalizadas no [ADR 002](../adr_002_trilhas_paralelas.md);
- A suíte de testes comprovou que variáveis de mesmo nome em trilhas distintas não colidem e que eventos sobrepostos são intercalados ordenadamente por instante de início.

### 3. Validação dos Testes de Estresse (INT-07)

Foram revisados os resultados dos testes de estresse:
- O compilador suportou com sucesso o agendamento de 64 eventos sonoros simultâneos sem corrupção de memória ou perda silenciosa de dados no buffer;
- As medições e limites de aceitação foram consolidados no documento [Teste de Estresse](../estresse.md).

### 4. Alinhamento da Documentação e README

O grupo realizou uma auditoria geral dos artefatos:
- Refatoração do `README.md` incluindo o pipeline completo com Análise Semântica e instruções de execução da suíte de testes e sanitizers;
- Registro de todas as atas de reunião semanais desde o início do semestre;
- Criação do documento formal de governança, políticas de contribuição e gestão de riscos.

### 5. Consolidação e Envio do Formulário do P2

Foi acordado o preenchimento conjunto e submissão formal do formulário de entrega do P2 da disciplina:
- O marco do P2 não prevê apresentação/ensaio presencial de banca com o professor nesta data; a avaliação ocorre pela submissão do formulário oficial de acompanhamento acompanhado da auditoria do repositório e do site documental pelo Prof. Sergio Freitas.
- O grupo reuniu todas as evidências requeridas no formulário: links dos PRs e issues fechadas na Sprint 04, suíte CTest com 22 testes de integração passando, job de AddressSanitizer (ASAN) sem vazamentos, matriz de rastreabilidade e link do [Relatório P2](../relatorio_p2.md).
- O envio do formulário foi programado para ser realizado dentro do prazo estipulado pelo professor no Teams/Plano de Ensino.

## Pendências / próximos passos

- [ ] Realizar o envio formal do formulário do P2 com as evidências e links consolidados
- [ ] Iniciar o planejamento da Sprint 05 (foco na saída final persistente COMP-C e polifonia avançada)
