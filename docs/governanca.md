---
title: Governança e Processo Ágil
layout: default
parent: Documentação
nav_order: 3
---

# Governança, Processo de Gestão e Práticas da Equipe

| | |
|---|---|
| **Projeto** | Compilador Musical (Grupo 03) |
| **Disciplina** | Compiladores 1 — Turma 01 (Prof. Sergio Freitas) — FCTE/UnB |
| **Equipe** | Arthur Luiz, Caio Melo Borges, Cecília Costa, Julia Oliveira, Marcella Anderle |
| **Repositório** | [Interpretador-Musical/Grupo03-COMP01](https://github.com/Interpretador-Musical/Grupo03-COMP01) |

---

## 1. Metodologia de Gestão

O desenvolvimento adota uma abordagem ágil inspirada no **Scrum**, com adaptações adequadas ao contexto acadêmico e ao ritmo das entregas e pontos de controle (P1, P2 e Entrega Final) da disciplina.

### Ciclos e Sprints

O cronograma do semestre foi estruturado em **6 Sprints** com duração típica de 2 a 3 semanas:

| Sprint | Período | Foco Principal | Marco de Entrega | Situação |
|---|---|---|---|---|
| **01** | Início de Agosto – 02/09 | Concepção, CMake, Flex/Bison inicial, setup de CI e infraestrutura | Setup & Base | Concluída |
| **02** | 03/09 – 23/09 | Scanner, Parser Bison, AST preliminar, integração end-to-end e áudio | **Ponto de Controle 1 (P1)** | Concluída |
| **03** | 24/09 – 14/10 | Modelagem temporal, divisão de arcos, estabilidade concorrente e design da AST | Baseline & AST | Concluída |
| **04** | 15/10 – 04/11 | Análise Semântica, Tabela de Símbolos, IR determinística, ASAN/TSAN e Trilhas Paralelas | **Ponto de Controle 2 (P2)** | Concluída |
| **05** | 05/11 – 18/11 | Geração da saída musical persistente (WAV), refinamento polifônico e testes ampliados | Estabilização & Saída | Planejada |
| **06** | 19/11 – 30/11 | Manual de uso, empacotamento, documentação final e ensaio para entrevistas | **Entrega Final** | Planejada |

---

## 2. Rituais e Comunicação

Para assegurar alinhamento técnico e cumprimento rigoroso das metas, a equipe adota rituais contínuos:

1. **Reuniões Semanais de Alinhamento (Quartas-feiras):** Encontros semanais (presenciais na FCTE/UnB ou remotos) para avaliação das tarefas da semana, decisões arquiteturais coletivas e planejamento da sprint seguinte. Todas as discussões são registradas em atas formais versionadas no repositório (`docs/atas/`).
2. **Checkpoints Assíncronos:** Utilização de canais de comunicação rápida para tirar dúvidas imediatas de integração e pareamento em bugs críticos.
3. **Quadro de Tarefas (Kanban no GitHub Projects):** Acompanhamento contínuo do fluxo de trabalho através dos estados: `Todo`, `In Progress`, `In Review`, `Testing` e `Done`.
4. **Revisões Cruzadas de Código (Code Review):** Nenhum pull request é integrado à branch `main` sem aprovação de ao menos um revisor e aprovação de todos os jobs automatizados da CI.

---

## 3. Matriz de Rotatividade de Papéis

Em conformidade com as diretrizes pedagógicas da disciplina de Compiladores 1, a equipe realiza **rotatividade periódica de papéis e responsabilidades** entre as frentes de trabalho:

- **Scrum Master (SM) / Gestão:** Facilita reuniões, monitora prazos, remove impedimentos e atualiza cronograma/atas.
- **Líder Técnico (Tech Lead / TL):** Define padrões arquiteturais, revisa design de interfaces e aprova PRs centrais.
- **Front-end do Compilador (CMP):** Responsável por Léxico (Flex), Sintático (Bison), gramática EBNF e Análise Semântica.
- **Core Matemático & Scheduler (MAT / SCH):** Responsável por estruturas de dados, Playhead, divisões temporais e filas de eventos.
- **Processamento de Áudio (DSP):** Responsável pela integração com `miniaudio`, decodificação de áudio, limitadores e síntese.
- **Qualidade, Testes & Documentação (QA / DOC):** Responsável por testes unitários (`doctest`), integração (CTest), sanitizers (ASAN/TSAN), relatórios de evidências e site.

### Histórico de Atribuição por Sprint

| Integrante | Sprint 01 | Sprint 02 (P1) | Sprint 03 | Sprint 04 (P2) | Sprint 05 (Prev.) | Sprint 06 (Prev.) |
|---|---|---|---|---|---|---|
| **Arthur Luiz** | Front-end (CMP) | Parser & AST (CMP) | Infraestrutura & CI | Front-end & AST (CMP) | Saída de Áudio (COMP-C) | Release & Apresentação |
| **Caio Melo Borges** | Gestão & Docs | Front-end & CLI (INT) | Core Matemático (MAT) | Semântica & Símbolos (CMP) | Refinamento DSP | Revisão Técnica |
| **Cecília Costa** | Setup & CMake | Documentação (DOC) | Scheduler (SCH) | Integração & Docs P2 (DOC/INT) | Testes End-to-End | Manual do Usuário |
| **Julia Oliveira** | Core & Tempo (MAT) | Engine & SPSC (SCH) | Design da AST (DOC) | Core & Qualidade ASAN (MAT) | Análise de Chiados | Relatório Final |
| **Marcella Anderle** | DSP & Áudio (DSP) | Envelopes & Áudio (DSP) | Mitigação Underrun (DSP) | Testes & Estresse (INT/MAT) | Polifonia & Limiter | Testes de Aceitação |

---

## 4. Critérios de Qualidade: DoR e DoD

Para manter alto padrão de engenharia de software e confiabilidade nas entregas, a equipe definiu critérios objetivos de entrada e saída:

### Definição de Preparado (Definition of Ready - DoR)
Uma issue está pronta para ser iniciada quando:
1. Possui título autoexplicativo no padrão do projeto (ex.: `CMP-13: Tabela de símbolos e análise semântica`);
2. Está vinculada a uma Sprint ativa e possui prioridade definida (P0, P1 ou P2);
3. Apresenta descrição clara do problema ou necessidade do compilador;
4. Contém critérios de aceitação verificáveis e mensuráveis;
5. Possui pelo menos um responsável atribuído (*assignee*).

### Definição de Pronto (Definition of Done - DoD)
Uma tarefa só é movida para `Done` quando:
1. O código segue os padrões do C++17 (RAII, ausência de ponteiros crus sem dono, tratamento defensivo de erros);
2. A funcionalidade é acompanhada por testes unitários automatizados em `doctest` cobrindo cenários nominais e de erro;
3. Todos os testes da suíte CTest passam com 100% de sucesso localmente e na CI;
4. O job de **AddressSanitizer (ASAN)** na CI comprova **zero vazamentos de memória** (*leak-free*);
5. O job de **ThreadSanitizer (TSAN)** na CI comprova **zero corridas de dados** (*data race-free*);
6. A documentação técnica correspondente (EBNF, especificação, arquitetura ou README) foi atualizada;
7. O Pull Request associado foi revisado, aprovado e integrado à branch `main`.

---

## 5. Política de Versionamento e Contribuição

### Padrão de Branches
- `main`: Branch de produção estável. Todo código na `main` deve estar compilando e com testes passando.
- `feat/<prefixo>-<descricao>`: Desenvolvimento de novas funcionalidades (ex.: `feat/cmp-07-trilhas-paralelas`).
- `fix/<prefixo>-<descricao>`: Correções de bugs ou ajustes em testes (ex.: `fix/int-06-mensagem-bpm`).
- `docs/<descricao>`: Alterações e atualizações na documentação e no site (ex.: `docs/doc-04-estrategia-testes-p2`).
- `release/<descricao>`: Preparação e congelamento de releases (ex.: `release/int-04-release-m1`).

### Padrão de Commits
Os commits seguem o padrão **Conventional Commits**:
- `feat(...)`: nova funcionalidade
- `fix(...)`: correção de bug
- `test(...)`: adição ou modificação de testes
- `docs(...)`: documentação ou atas
- `build(...)` / `ci(...)`: ajustes no CMake ou GitHub Actions
- `refactor(...)`: refatoração de código sem alteração de comportamento

---

## 6. Gestão e Matriz de Riscos

| Risco Identificado | Impacto | Probabilidade | Estratégia de Mitigação Adotada |
|---|---|---|---|
| **Fuga de escopo da disciplina:** Tratar o projeto como ferramenta de live coding em vez de compilador formal. | Alto | Baixa (inicialmente Média) | Adoção da **decisão batch (ADR 001)**: o arquivo `.mus` é finito e completamente processado até a representação intermediária antes de qualquer áudio. |
| **Vazamentos de memória no Bison:** Descarte de nós da AST em erros sintáticos gerando leaks. | Médio | Alta | Configuração de diretivas `%destructor` no `parser.y`, uso de `std::variant` com encapsulamento RAII e validação via **AddressSanitizer** na CI. |
| **Estouro de pilha (Stack Overflow):** Programas `.mus` com aninhamento recursivo malicioso ou excessivo. | Alto | Média | Implementação de teto determinístico de **256 níveis de profundidade na AST (CMP-08)** com erro amigável de compilação. |
| **Distorção e Clipping Acústico:** Múltiplas notas tocando simultaneamente em trilhas paralelas causando saturação. | Médio | Alta | Inclusão de **limitador suave (soft-clipper) no master bus (DSP-10)** e envelopes de ataque e liberação de 5 ms nas notas. |
| **Sobrecarga em semanas de avaliação:** Atrasos por acúmulo de provas e entregas concomitantes. | Alto | Alta | Planejamento em Sprints com marcos antecipados, atas semanais de cobrança e escopo defensável (adiamento de dot-chaining e condicionais fora do P2). |
