---
title: Documentação
layout: default
nav_order: 2
has_children: true
---

# Documentação

Este espaço reúne as decisões arquiteturais, especificações, evidências e orientações de uso do **Compilador Musical Batch**. O programa `.mus` é finito e sua Timeline é produzida antes da reprodução. As páginas marcadas como relatórios históricos registram a situação do commit de referência; o [Roadmap]({{ site.baseurl }}{% link roadmap.md %}) apresenta as prioridades atuais.

## Gestão, governança e escopo

- [Governança e Processo Ágil]({{ site.baseurl }}{% link governanca.md %}) — metodologia Scrum adaptada, rituais, rotatividade de papéis, DoR/DoD e matriz de riscos.
- [Cronograma]({{ site.baseurl }}{% link cronograma.md %}) — planejamento do semestre e entregas por sprint.
- [Roadmap]({{ site.baseurl }}{% link roadmap.md %}) — capacidades integradas, pendências e decisões de priorização.
- [ADR 001 — Compilação batch]({{ site.baseurl }}{% link decisao_modelo_batch.md %}) — registro da decisão original de 16/09, alternativas e matriz histórica.
- [ADR 002 — Trilhas paralelas finitas]({{ site.baseurl }}{% link adr_002_trilhas_paralelas.md %}) — decisão de 02/11 e contrato de polirritmia de bloco integrado no PR #128.
- [Diretrizes de escopo e prioridades]({{ site.baseurl }}{% link decisoes_escopo_final.md %}) — prioridades P2/Sprint 05/Sprint 06, limites de escopo e situação da MAT-06.

## Referência técnica e uso

- [Visão de arquitetura]({{ site.baseurl }}{% link arquitetura.md %}) — pipeline de compilação e execução de áudio.
- [Especificação de requisitos]({{ site.baseurl }}{% link especificacao.md %}) — comandos, valores, semântica e restrições da DSL.
- [Gramática EBNF]({{ site.baseurl }}{% link gramatica_ebnf.md %}) — correspondência com o parser Bison.
- [Arquitetura da AST]({{ site.baseurl }}{% link ast.md %}) — organização, nós, memória e profundidade.
- [Representação intermediária]({{ site.baseurl }}{% link representacao_intermediaria.md %}) — Timeline de SoundEvent, invariantes e ordenação estável.
- [Guia de ambiente]({{ site.baseurl }}{% link guia_ambiente.md %}) — build e ferramentas.
- [Cálculos e cobertura]({{ site.baseurl }}{% link calculos.md %}) — matemática do pipeline e testes associados.

## Evidências, qualidade e entregas

- [Relatório e evidências P2]({{ site.baseurl }}{% link relatorio_p2.md %}) — fotografia documental integrada pelo PR #121.
- [Testes de estresse]({{ site.baseurl }}{% link estresse.md %}) — cargas, medições, limites de aceitação e defeitos identificados.
- [Release M1]({{ site.baseurl }}{% link release_m1.md %}) — histórico da primeira release.

## Atas de reunião

| Data | Registro / Pauta Principal |
|---|---|
| [19/08/2026]({{ site.baseurl }}{% link atas/2026-08-19-ata-reuniao.md %}) | Concepção da ideia, escolha de C++/CMake, miniaudio e conceito de playhead |
| [26/08/2026]({{ site.baseurl }}{% link atas/2026-08-26-ata-reuniao.md %}) | Decisão por Flex e Bison, regras de repositório e composição do grupo |
| [02/09/2026]({{ site.baseurl }}{% link atas/2026-09-02-ata-reuniao.md %}) | Fechamento Sprint 01, GitHub Pages, CMake e prefixos de issues |
| [09/09/2026]({{ site.baseurl }}{% link atas/2026-09-09-ata-reuniao.md %}) | Primeira emissão sonora via miniaudio, playhead e loop monotônico |
| [16/09/2026]({{ site.baseurl }}{% link atas/2026-09-16-ata-reuniao.md %}) | **Decisão de Arquitetura Batch (ADR 001)**, testes com doctest/CTest e CI com TSAN |
| [23/09/2026]({{ site.baseurl }}{% link atas/2026-09-23-ata-reuniao.md %}) | **Entrega do Ponto de Controle 1 (P1)**, pipeline conectado e ring buffer SPSC |
| [30/09/2026]({{ site.baseurl }}{% link atas/2026-09-30-ata-reuniao.md %}) | Retrospectiva P1, planejamento Sprint 03 e atualização documental do site |
| [07/10/2026]({{ site.baseurl }}{% link atas/2026-10-07-ata-reuniao.md %}) | Design de memória da AST, mitigação de underrun e modelagem de escopos |
| [14/10/2026]({{ site.baseurl }}{% link atas/2026-10-14-ata-reuniao.md %}) | Fechamento Sprint 03 e planejamento estratégico da **Sprint 04 (foco no P2)** |
| [21/10/2026]({{ site.baseurl }}{% link atas/2026-10-21-ata-reuniao.md %}) | Tabela de Símbolos, tipos estáticos, flag `--simbolos` e 6 invariantes da IR |
| [28/10/2026]({{ site.baseurl }}{% link atas/2026-10-28-ata-reuniao.md %}) | Homologação da Release M1, ASAN na CI e decisão sobre Trilhas Paralelas |
| [04/11/2026]({{ site.baseurl }}{% link atas/2026-11-04-ata-reuniao.md %}) | Pauta e minuta: fechamento Sprint 04, testes de estresse e formulário do P2 |

## Trabalho futuro não obrigatório para o P2

O manual do usuário (DOC-05 #32), a saída-alvo musical definitiva (COMP-C #108), a atualização arquitetural final (INT-11 #63) e o relatório final (DOC06 #38) seguem as prioridades das Sprints 05 e 06. O projeto não pressupõe live coding, hot-swapping, mini-notação ou filtros avançados como requisitos essenciais.
