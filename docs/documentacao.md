---
title: Documentação
layout: default
nav_order: 2
has_children: true
---

# Documentação

Este espaço reúne as decisões arquiteturais, especificações, evidências e orientações de uso do **Compilador Musical Batch**. O programa `.mus` é finito e sua Timeline é produzida antes da reprodução. As páginas marcadas como relatórios históricos registram a situação do commit de referência; o [Roadmap]({{ site.baseurl }}{% link roadmap.md %}) apresenta as prioridades atuais.

## Decisões e escopo

- [ADR 001 — Compilação batch]({{ site.baseurl }}{% link decisao_modelo_batch.md %}) — registro da decisão original de 16/09, alternativas e matriz histórica.
- [ADR 002 — Trilhas paralelas finitas]({{ site.baseurl }}{% link adr_002_trilhas_paralelas.md %}) — decisão de 02/11 e contrato de polirritmia de bloco integrado no PR #128.
- [Diretrizes de escopo e prioridades]({{ site.baseurl }}{% link decisoes_escopo_final.md %}) — prioridades P2/Sprint 05/Sprint 06, limites de escopo e situação da MAT-06.
- [Cronograma]({{ site.baseurl }}{% link cronograma.md %}) — planejamento do semestre e entregas.
- [Roadmap]({{ site.baseurl }}{% link roadmap.md %}) — capacidades integradas, pendências e decisões de priorização.

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

| Data | Registro |
|---|---|
| [19/08/2026]({{ site.baseurl }}{% link atas/2026-08-19-ata-reuniao.md %}) | Concepção, stack e pipeline |
| [26/08/2026]({{ site.baseurl }}{% link atas/2026-08-26-ata-reuniao.md %}) | Flex/Bison, organização e pendências |

## Trabalho futuro não obrigatório para o P2

O manual do usuário (DOC-05 #32), a saída-alvo musical (COMP-C #108), a atualização arquitetural final (INT-11 #63) e o relatório final (DOC06 #38) seguem as prioridades das Sprints 05 e 06. O projeto não pressupõe live coding, hot-swapping, mini-notação ou filtros avançados como requisitos essenciais.
