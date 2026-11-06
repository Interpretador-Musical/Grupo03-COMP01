---
title: Cronograma
layout: default
parent: Documentação
nav_order: 3
---

# Cronograma do Compilador Musical — Escopo Batch (revisado em 05/11/2026)

## Referências e estratégia

Este cronograma é orientado pela [especificação]({{ site.baseurl }}{% link especificacao.md %}) e pela [arquitetura]({{ site.baseurl }}{% link arquitetura.md %}) do Grupo 03, alinhadas à proposta e ao **Guia de Projeto de um Compilador** do professor Sergio Freitas. A decisão de 16/09/2026 estabelece que cada arquivo `.mus` é um programa musical finito, analisado integralmente antes da reprodução. A `Timeline` de `SoundEvent` é a **representação intermediária do domínio**, formalizada na COMP-A (#104) e emitida em texto determinístico pela COMP-B (#105); a forma da saída musical final é especificada e validada na Sprint 05.

O pipeline do produto é: arquivo `.mus` → Flex (tokens) → Bison (AST) → análise semântica e tabela de símbolos → representação intermediária musical → geração de saída musical → motor de execução/reprodução. O módulo `Interpretador` percorre a AST e traduz cada comando em eventos da `Timeline`: é a etapa de geração da representação intermediária. O que caracteriza o projeto como compilador é essa tradução completa do programa para uma IR verificável antes de qualquer reprodução, como registrado no ADR da decisão batch (DOC-B, #107).

## Marcos

| Sprint | Período | Foco realinhado | Situação em 05/11 |
|---|---|---|---|
| 01 | início de agosto–02/09 | ambiente, DSL, Flex e estrutura inicial | encerrada |
| 02 | 03/09–23/09 | parser, AST inicial, P1 e integração básica | encerrada |
| 03 | 24/09–14/10 | AST, semântica, modelagem temporal e baseline | encerrada no calendário; pendências foram carregadas |
| 04 | 15/10–04/11 | semântica, IR musical, gramática formal, testes e P2 | encerrada; todos os itens integrados |
| 05 | 05/11–18/11 | geração de saída final, integração, testes e estabilização | em andamento desde 05/11 |
| 06 | 19/11–30/11 | release, documentação, entrevistas e entrega | planejada; confirmar prazos finais |

**Aviso:** marcos P2 e entrega final devem ser confrontados com o Plano de Ensino e Teams. Os intervalos acima preservam o planejamento local; não substituem comunicações oficiais.

## Sprint 01 — Base do compilador (histórico)

Concluídas: CMP-01 #2, MAT-01 #3, SCH-01 #4, DSP-01 #6, INT-01 #7, DOC-01 #8, CMP-02 #9, MAT-02 #10, SCH-02 #11, DSP-02 #12. A leitura de `.mus` pelo terminal (INT-02 #13) está no fluxo principal do `compilador`.

## Sprint 02 — Scanner, parser e P1 (histórico)

Concluídas: DOC-02 #14, CMP-03 #15, MAT-03 #16, SCH-03 #17, DSP-03 #18, INT-03 #19. As evidências do pipeline e do P1 permanecem parte da rastreabilidade.

## Sprint 03 — Representações e baseline (histórico)

Concluídas conforme estado das issues: DOC-03 #20, MAT-05 #28 e INT-04 #25. A análise semântica CMP-13 #95 foi integrada e fechada na Sprint 04. Pendências não concluídas (#21, #22, #24, #29, #30, #31 etc.) são replanejadas conforme prioridade atual.

## Sprint 04 — Código intermediário e P2 (encerrada em 04/11)

**Objetivo:** demonstrar formalmente as etapas do compilador exigidas para o P2, com progresso e decisões defensáveis.

| Item | Entrega | Prioridade | Verificação |
|---|---|---|---|
| CMP-13 #95 | Tabela de símbolos/análise semântica | concluída | testes + flag `--simbolos` |
| COMP-A #104 | Definição da timeline como IR musical | concluída (PR #114) | contrato documentado e testes AST→IR |
| COMP-B #105 | Expor IR como saída textual determinística | concluída (PR #113) | `--emit-timeline` + testes CTest |
| DOC-A #106 | EBNF da sintaxe implementada | concluída (PR #116) | comparação com Flex/Bison |
| DOC-B #107 | ADR do escopo batch e reclassificação do backlog | concluída (PR #117) | justificativa e links de rastreabilidade |
| INT-06 #37 | Testes end-to-end `.mus`→timeline→execução | concluída (PR #115) | testes CLI/CTest |
| MAT-04 #22 | Precisão numérica dos tempos e durações | concluída (PR #112) | testes de playhead e interpretador |
| CMP-08 #44 | Edge cases e limite de profundidade da AST | concluída (PR #100) | testes de aninhamento |
| MAT-11 #60 | Auditoria de memória (ASAN) | concluída (PR #99) | job de AddressSanitizer na CI |
| DSP-10 #57 | Limitador no master bus | concluída (PR #101) | testes de pico |
| DSP-04 #24 | Sinalização de underrun na fila de eventos | concluída (PR #103) | testes de underrun |
| DOC-04 #26 | Estratégia de testes e formulário P2 | concluída (PR #121) | evidências do progresso P1→P2 |
| CMP-04 #21 | AST completa e íntegra, sobre a MAT-11 e a CMP-08 | concluída (PR #130, como auditoria e testes) | testes de aninhamento e ASAN |
| CMP-07 #39 | Trilhas paralelas `{ , }`: a linguagem passa a gerar eventos simultâneos | concluída (PR #128) | EBNF comparada ao Bison, `tests/test_trilhas.cpp`, `tests/ir/trilhas.mus` |

**Critério de conclusão do P2:** equipe consegue demonstrar Flex, Bison, AST, semântica e uma timeline inspecionável ou com seus eventos demonstrados por testes, com registro explícito das lacunas até a geração final. O professor prioriza progresso real, não promessas de funcionalidades futuras.

## Sprint 05 — Saída final, estabilidade e integração (05/11–18/11)

**Objetivo:** entregar uma cadeia de compilação musical com saída-alvo documentada e testes de reprodução.

| Item | Entrega | Prioridade |
|---|---|---|
| NOVA COMP-C | Decidir e implementar saída final (artefato musical/áudio) | P0 |
| DSP-05 #30 | Validação da polifonia com testes | entregue antes, no PR #118 (02/11) |
| BUG-AUDIO | Diagnóstico reproduzível dos chiados relatados | P1 |
| SCH-05 #29 | Integridade do buffer SPSC | entregue antes, no PR #120 (02/11) |
| MAT-08 #45 | Drift temporal | P1 |
| INT-07 #43 | Stress com BPM e densidade altos | entregue antes, no PR #126 (02/11); os achados viraram #123, #124 e #125 |
| INT-05 #31 (revisão) | Revisar o PR #137 (samples), integrado em 04/11, antes das próximas tasks | P1 |
| INT-11 #63 (iniciar) | Atualizar arquitetura com IR e saída final | P1 |
| MAT-07 #40 | Testes matemáticos ampliados | entregue antes, no PR #122 (02/11) |
| NOVA COMP-D | Otimização intermediária opcional | P2 |

Condicionais (`se/entao/senao`) e funções (`defina/retorna`) permanecem **planejadas**, não concluídas: só priorizar após decisão explícita do grupo e garantia de tempo para testes da cadeia principal. O guia do professor não obriga essas construções caso não façam parte da linguagem definida.

## Sprint 06 — Entrega final e entrevistas (19/11–30/11)

**Objetivo:** consolidar uma versão demonstrável, explicar as decisões técnicas e entregar documentação completa.

| Item | Entrega |
|---|---|
| INT-12 #68 | Versão final e demonstração do compilador musical |
| INT-11 #63 | README, manual de build/uso, arquitetura e formato de saída |
| DOC06 #38 | Relatório final e rastreabilidade das decisões |
| CMP-12 #64 | Revisão de warnings/análise estática proporcional ao risco |
| DSP-12 #67 | Validação sonora final, caso necessária |
| MAT-12 #65 | Regressão matemática final |

Otimizações DSP complexas, BiQuad LPF (#62), açúcar sintático (#59), auditorias avançadas não essenciais e melhorias de UI ficam fora do caminho crítico, podendo ser encerradas ou movidas ao backlog futuro mediante decisão do grupo.

## Backlog fora de escopo (histórico preservado)

Encerrar como `not planned`, após verificar dependências/PRs: SCH-04 #23, SCH-06 #35, CMP-09 #49, MAT-09 #50, SCH-09 #51, INT-09 #53, CMP-10 #54, SCH-10 #56, INT-10 #58. Não apagá-las do repositório.

CMP-05 #27 e CMP-06 #33 foram fechadas com testes (PRs #131 e #132), sem mini-notação nem dot-chaining; a MAT-06 #34 entregou só a infraestrutura de ambientes (PR #129). A CMP-07 #39 foi decidida em 02/11 e entrou na Sprint 04. O fato de estarem relacionadas a padrões funcionais ou chaining não as torna requisitos do compilador batch.

## Gestão das issues

Cada tarefa ativa deve ter: objetivo, referência ao requisito, entregáveis, critérios de aceite verificáveis, sprint, prioridade, assignee e PR/commit associado. Status: `Todo`, `In progress`, `In review`, `Testing`, `Done`; `not planned` fora do quadro ativo. Uma tarefa passa a `Done` quando o PR associado é integrado à `main`.

## Fontes

- [Guia de compilador do professor](https://github.com/sergioaafreitas/COMP1/blob/main/semana%2001/docs/Guia%20-%20Projeto%20de%20um%20compilador.md)
- [Proposta geral do trabalho](https://github.com/sergioaafreitas/COMP1/blob/main/semana%2001/docs/Trabalho%20de%20Compiladores.md)
- [Arquitetura do Grupo 03]({{ site.baseurl }}{% link arquitetura.md %})
- [Especificação de requisitos]({{ site.baseurl }}{% link especificacao.md %})
