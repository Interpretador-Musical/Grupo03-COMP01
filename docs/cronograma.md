---
title: Cronograma
layout: default
parent: Documentação
nav_order: 3
---

# Cronograma do Compilador Musical — Escopo Batch (revisado em 29/10/2026)

## Referências e estratégia

Este cronograma é orientado pela [especificação]({{ site.baseurl }}{% link especificacao.md %}) e pela [arquitetura]({{ site.baseurl }}{% link arquitetura.md %}) do Grupo 03, alinhadas à proposta e ao **Guia de Projeto de um Compilador** do professor Sergio Freitas. A decisão de 16/09/2026 estabelece que cada arquivo `.mus` é um programa musical finito, analisado integralmente antes da reprodução. A geração de uma `Timeline` de `SoundEvent` constitui uma **candidata a representação intermediária do domínio**, a ser formalizada; a forma de geração da saída musical final será especificada e validada nas Sprints 4–5.

O pipeline do produto é: arquivo `.mus` → Flex (tokens) → Bison (AST) → análise semântica e tabela de símbolos → representação intermediária musical → geração de saída musical → motor de execução/reprodução. A existência de um módulo denominado `Interpretador` que percorre a AST não é ocultada; ele constitui a implementação atual de avaliação/tradução para eventos. A documentação deve explicar essa decisão tecnicamente, e não depender da afirmação de que batch implica compilador.

## Marcos

| Sprint | Período | Foco realinhado | Situação em 29/10 |
|---|---|---|---|
| 01 | início de agosto–02/09 | ambiente, DSL, Flex e estrutura inicial | encerrada; #13 requer reconciliação |
| 02 | 03/09–23/09 | parser, AST inicial, P1 e integração básica | encerrada |
| 03 | 24/09–14/10 | AST, semântica, modelagem temporal e baseline | encerrada no calendário; pendências foram carregadas |
| 04 | 15/10–04/11 | semântica, IR musical, gramática formal, testes e P2 | em andamento |
| 05 | 05/11–18/11 | geração de saída final, integração, testes e estabilização | planejada |
| 06 | 19/11–30/11 | release, documentação, entrevistas e entrega | planejada; confirmar prazos finais |

**Aviso:** marcos P2 e entrega final devem ser confrontados com o Plano de Ensino e Teams. Os intervalos acima preservam o planejamento local; não substituem comunicações oficiais.

## Sprint 01 — Base do compilador (histórico)

Concluídas: CMP-01 #2, MAT-01 #3, SCH-01 #4, DSP-01 #6, INT-01 #7, DOC-01 #8, CMP-02 #9, MAT-02 #10, SCH-02 #11, DSP-02 #12. Revisar INT-02 #13 antes do encerramento: leitura de `.mus` já está no fluxo principal.

## Sprint 02 — Scanner, parser e P1 (histórico)

Concluídas: DOC-02 #14, CMP-03 #15, MAT-03 #16, SCH-03 #17, DSP-03 #18, INT-03 #19. As evidências do pipeline e do P1 permanecem parte da rastreabilidade.

## Sprint 03 — Representações e baseline (histórico)

Concluídas conforme estado das issues: DOC-03 #20, MAT-05 #28 e INT-04 #25. A análise semântica CMP-13 #95 foi integrada e fechada na Sprint 04. Pendências não concluídas (#21, #22, #24, #29, #30, #31 etc.) são replanejadas conforme prioridade atual.

## Sprint 04 — Código intermediário e P2 (até 04/11)

**Objetivo:** demonstrar formalmente as etapas do compilador exigidas para o P2, com progresso e decisões defensáveis.

| Item | Entrega | Prioridade | Verificação |
|---|---|---|---|
| CMP-13 #95 | Tabela de símbolos/análise semântica | concluída | testes + flag `--simbolos` |
| NOVA COMP-A | Definição da timeline como IR musical | P0 | contrato documentado e testes AST→IR |
| NOVA DOC-A | EBNF da sintaxe implementada | P0 | comparação com Flex/Bison |
| INT-06 #37 (reformular) | Testes end-to-end `.mus`→timeline→execução | P0 | testes CLI/CTest |
| DOC-04 #26 (reformular) | Estratégia de testes e formulário P2 | P0 | evidências do progresso P1→P2 |
| NOVA DOC-B ou item #26 | ADR do escopo batch e reclassificação do backlog | P0 | justificativa e links de rastreabilidade |
| NOVA COMP-B | Expor IR como saída textual determinística | P0/P1 | testes; iniciar antes de P2 se viável |
| CMP-04 #21 | Robustez da AST | P1 | testes de aninhamento, revisão do PR #100 |
| DSP-05 #30 | Revisão de polifonia | P1 | PR #94 + testes; não bloquear P2 sem necessidade |

**Critério de conclusão do P2:** equipe consegue demonstrar Flex, Bison, AST, semântica e uma timeline inspecionável ou com seus eventos demonstrados por testes, com registro explícito das lacunas até a geração final. O professor prioriza progresso real, não promessas de funcionalidades futuras.

## Sprint 05 — Saída final, estabilidade e integração (05/11–18/11)

**Objetivo:** entregar uma cadeia de compilação musical com saída-alvo documentada e testes de reprodução.

| Item | Entrega | Prioridade |
|---|---|---|
| NOVA COMP-C | Decidir e implementar saída final (artefato musical/áudio) | P0 |
| NOVA COMP-B (concluir) | Emissão auditável do IR | P1 |
| DSP-05 #30 | Homologar mixagem e integrar PR #94 | P1 |
| BUG-AUDIO | Diagnóstico reproduzível dos chiados relatados | P1 |
| DSP-04 #24 | Corrigir underruns quando confirmados | P1 |
| SCH-05 #29 | Integridade do buffer SPSC | P1 |
| MAT-04 #22, MAT-08 #45 | Precisão e drift temporal | P1 |
| INT-07 #43 | Stress com BPM e densidade altos | P1 |
| INT-11 #63 (iniciar) | Atualizar arquitetura com IR e saída final | P1 |
| MAT-07 #40 | Testes matemáticos ampliados | P2 |
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
| MAT-11 #60 / PR #99 | Auditoria de memória e integração segura, conforme revisão |
| DSP-12 #67 | Validação sonora final, caso necessária |
| MAT-12 #65 | Regressão matemática final |

Otimizações DSP complexas, BiQuad LPF (#62), açúcar sintático (#59), auditorias avançadas não essenciais e melhorias de UI ficam fora do caminho crítico, podendo ser encerradas ou movidas ao backlog futuro mediante decisão do grupo.

## Backlog fora de escopo (histórico preservado)

Encerrar como `not planned`, após verificar dependências/PRs: SCH-04 #23, SCH-06 #35, CMP-09 #49, MAT-09 #50, SCH-09 #51, INT-09 #53, CMP-10 #54, SCH-10 #56, INT-10 #58. Não apagá-las do repositório.

Revisar antes de decidir: CMP-05 #27, CMP-06 #33, MAT-06 #34, CMP-07 #39. O fato de estarem relacionadas a padrões funcionais ou chaining não as torna requisitos do compilador batch.

## Gestão das issues

Cada tarefa ativa deve ter: objetivo, referência ao requisito, entregáveis, critérios de aceite verificáveis, sprint, prioridade, assignee e PR/commit associado. Status: `Todo`, `In progress`, `In review`, `Testing`, `Done`; `not planned` fora do quadro ativo. Um PR aberto não representa entrega concluída.

## Fontes

- [Guia de compilador do professor](https://github.com/sergioaafreitas/COMP1/blob/main/semana%2001/docs/Guia%20-%20Projeto%20de%20um%20compilador.md)
- [Proposta geral do trabalho](https://github.com/sergioaafreitas/COMP1/blob/main/semana%2001/docs/Trabalho%20de%20Compiladores.md)
- [Arquitetura do Grupo 03]({{ site.baseurl }}{% link arquitetura.md %})
- [Especificação de requisitos]({{ site.baseurl }}{% link especificacao.md %})
