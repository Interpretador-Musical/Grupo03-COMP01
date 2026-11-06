---
title: "Pauta e Minuta — 04/11/2026"
layout: default
parent: Documentação
nav_order: 32
---

# Pauta e Minuta de Reunião — 04/11/2026

- **Projeto:** Interpretador de linguagem de programação musical
- **Disciplina:** Compiladores 1 — Turma 01 (Prof. Sergio) — FCTE/UnB
- **Horário:** 14:00 – 16:00
- **Local / formato:** Sala da FCTE / Presencial
- **Ata anterior:** [28/10/2026](./2026-10-28-ata-reuniao.md)
- **Status:** Pauta preliminar / Minuta para deliberação da equipe

## Participantes

- Arthur Luiz
- Caio Melo Borges
- Cecília Costa
- Julia Oliveira
- Marcella Anderle

## Pauta

1. Fechamento formal da **Sprint 04** e balanço das metas da equipe
2. Homologação das **Trilhas Paralelas Finitas (CMP-07 / ADR 002)**
3. Verificação dos resultados da suíte de integração ponta a ponta (INT-06) e estresse (INT-07)
4. Aprovação e conferência do **Relatório de Verificação e Evidências do P2 (DOC-04)**
5. Auditoria da documentação: governança, atas completas e refatoração do README (PR #135)
6. Alinhamento das respostas e envio do **Formulário de Avaliação do P2**

## Tópicos para deliberação na reunião

### 1. Balanço da Sprint 04 e critérios avaliativos do P2

Apresentação e conferência das entregas da Sprint 04 frente aos critérios avaliativos do P2:
- Front-end completo: Flex (`lexer.l`), Bison (`parser.y`) e Gramática EBNF correspondente;
- AST robusta em C++17 com segurança de tipos, controle estrito de ciclo de vida e limite determinístico de profundidade de 256 níveis;
- Análise Semântica com Tabela de Símbolos, resolução de escopos e checagem estática de tipos, acessível via flag `--simbolos`;
- Representação Intermediária (`Timeline`) formalizada com validação das 6 invariantes e exportação determinística em CSV via `--emit-timeline`;
- Pipeline de testes automatizados com 22 testes no CTest, com validação de ausência de vazamentos na suíte de testes via AddressSanitizer (ASAN) e ausência de corridas de dados via ThreadSanitizer (TSAN) nos fluxos cobertos na CI.

### 2. Homologação das Trilhas Paralelas (CMP-07 / ADR 002)

Validação coletiva da integração de trilhas paralelas `{ trilha1, trilha2, ... }`:
- Permite acordes, polirritmias e vozes simultâneas com cursores de tempo independentes;
- As regras de semântica (isolamento de variáveis e proibição de alteração global de andamento dentro de blocos paralelos) foram formalizadas no [ADR 002](../adr_002_trilhas_paralelas.md);
- Submeter à equipe os testes comprovando que variáveis de mesmo nome em trilhas distintas não colidem e que eventos sobrepostos são intercalados ordenadamente por instante de início.

### 3. Verificação dos Testes de Estresse (INT-07)

Apresentação dos resultados da suíte sob carga:
- O compilador suportou o agendamento de 64 eventos sonoros simultâneos sem corrupção de memória ou perda silenciosa de dados no buffer;
- As medições e limites de aceitação estão registrados no documento [Teste de Estresse](../estresse.md).

### 4. Auditoria da Documentação e Atendimento à Revisão do PR #135

Deliberação sobre os ajustes no PR #135:
- Refatoração do `README.md` com a inclusão da Análise Semântica no pipeline, tabela de opções da CLI, precisão na separação de papéis entre Lexer e Parser, e correção das instruções de build com sanitizers;
- Registro cronológico de todas as atas de reunião semanais desde o início do semestre;
- Documento formal de governança, políticas de contribuição e gestão de riscos (`docs/governanca.md`).

### 5. Consolidação e Envio do Formulário do P2

Alinhamento do preenchimento e submissão formal do formulário de entrega do P2 da disciplina:
- O marco do P2 avalia o compilador por meio do formulário oficial de acompanhamento acompanhado da auditoria do repositório e do site documental pelo Prof. Sergio Freitas;
- O grupo reunirá todas as evidências no formulário: links dos PRs e issues fechadas na Sprint 04, suíte CTest com 22 testes de integração passando, job de AddressSanitizer (ASAN) sem vazamentos na suíte, matriz de rastreabilidade e link do [Relatório P2](../relatorio_p2.md);
- Definição do responsável pela submissão formal dentro do prazo estipulado no Teams/Plano de Ensino.

## Pendências / próximos passos

- [ ] Realizar a reunião semanal e lavrar o registro formal de deliberações após a sessão
- [ ] Realizar o envio formal do formulário do P2 com as evidências e links aprovados
- [ ] Iniciar o planejamento da Sprint 05 (foco na saída final persistente COMP-C e polifonia avançada)
