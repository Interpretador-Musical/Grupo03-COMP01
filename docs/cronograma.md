---
title: Cronograma
layout: default
parent: Documentação
nav_order: 3
---

# Cronograma e rastreabilidade — Interpretador Musical

> **Revisão de escopo (29/10/2026).** Este cronograma substitui o planejamento técnico inicial baseado em padrões musicais infinitos e live coding. Conforme decisão de 16/09, consolidada em [Visão de Arquitetura]({{ site.baseurl }}{% link arquitetura.md %}), o produto é uma DSL musical de **programas finitos**, lida e validada em lote (*batch*), cuja AST é interpretada para produzir uma timeline de eventos e, em seguida, som. Features de hot-swapping, lookahead de padrões infinitos e edição durante execução **não integram o caminho crítico**. As respectivas issues são preservadas no histórico, mas devem ser encerradas como `not planned` após revisão.

## 1. Referências e critérios

- [Especificação da Linguagem]({{ site.baseurl }}{% link especificacao.md %}): requisitos e sintaxe.
- [Arquitetura]({{ site.baseurl }}{% link arquitetura.md %}): pipeline, decisões e modelo batch.
- [Arquitetura da AST]({{ site.baseurl }}{% link ast.md %}): representação interna.
- [Notas da Release M1]({{ site.baseurl }}{% link release_m1.md %}): baseline entregue em 27/10.
- [Proposta de Trabalho — Compiladores 1](https://github.com/sergioaafreitas/COMP1/blob/main/semana%2001/docs/Trabalho%20de%20Compiladores.md): P1, P2, GitHub, documentação e entrevista.
- [Guia de Projeto de Interpretador](https://github.com/sergioaafreitas/COMP1/blob/main/semana%2001/docs/Guia%20-%20Projeto%20de%20um%20interpretador.md): Flex, Bison, AST, semântica e interpretação.

**Classificação de entregas:** **obrigatória** (essencial ao produto e avaliação); **condicionada** (planejada pelo grupo, depende de decisão de escopo/capacidade); **opcional** (qualidade adicional, DSP e otimização). A interpretação da AST atende ao caminho de **interpretador** da disciplina; geração de TAC/assembly não será acrescentada artificialmente.

## 2. Resumo dos marcos

| Período | Datas adotadas pelo grupo | Meta | Situação em 29/10/2026 |
|---|---|---|---|
| Sprint 01 | início de agosto–02/09 | Equipe, linguagem, ambiente, lexer inicial | Concluída; revisar fechamento de INT-02 #13 |
| Sprint 02 | 03/09–23/09 | Parser, AST inicial, fluxo end-to-end, P1 | Concluída |
| Sprint 03 | 24/09–14/10 | AST, tabela de símbolos, semântica, áudio e testes | Marco encerrado; pendências realocadas |
| **Sprint 04** | **15/10–04/11** | **Conformidade técnica, evidências e entrega P2** | **Em andamento; prioridade máxima** |
| Sprint 05 | 05/11–18/11 | Estabilização, testes integrados e extensões aprovadas | Planejada |
| Sprint 06 | 19/11–30/11 | Release final, documentação, preparação para entrevistas | Planejada; confirmar prazos externos |

> Datas das apresentações, entrega final via Teams e entrevistas devem ser confirmadas com o Plano de Ensino/Teams. A proposta do professor apresenta sprints sugeridas e exige consistência dos dados enviados no P1/P2; **somente um membro** submete o formulário.

## 3. Critérios de aceite globais

Uma issue é concluída apenas após: (a) requisito identificado, (b) código/artefato integrado à `main`, quando aplicável, (c) testes e evidência de execução, (d) documentação coerente e (e) PR revisado. Um PR aberto ou teste local informal **não é** entrega concluída. Para documentação sem código, evidência é PR aprovado, revisão e material publicado.

### Pipeline mínimo da demonstração

`arquivo .mus → Flex → Bison → AST → tabela de símbolos/análise semântica → interpretador → Timeline/SoundEvent → miniaudio`

A execução deve rejeitar programas inválidos **antes** de reproduzir som e deve permitir demonstração sem dispositivo de áudio via `--no-audio`.

## 4. Sprint 01 — Fundação e linguagem (histórico)

**Entregas preservadas:** CMP-01 #2; MAT-01 #3; SCH-01 #4; DSP-01 #6; INT-01 #7; DOC-01 #8; CMP-02 #9; MAT-02 #10; SCH-02 #11; DSP-02 #12.

**Pendência administrativa:** INT-02 #13, ainda aberta, deverá ser confrontada com a leitura de arquivos `.mus` já implementada no fluxo da INT-03. Fechar como concluída se critérios forem comprovados, ou encerrar a parte interativa fora de escopo; não duplicar feature.

## 5. Sprint 02 — Lexer, parser, AST e P1 (histórico)

**Entregas preservadas:** DOC-02 #14, CMP-03 #15, MAT-03 #16, SCH-03 #17, DSP-03 #18, INT-03 #19.

**Resultado:** fluxo end-to-end de arquivo musical a timeline/áudio, CLI, mensagens com posições e base de testes. As entregas efetivamente integradas são demonstráveis com exemplos `.mus`.

## 6. Sprint 03 — Estrutura interna, semântica e baseline (histórico com pendências realocadas)

**Concluídas identificadas:** DOC-03 #20, MAT-05 #28, INT-04 #25 e CMP-13 #95 (implementação de tabela de símbolos/análise semântica vinculada ao núcleo).

**Revisão de pendências:** CMP-04 #21 (AST/aninhamento), MAT-04 #22 (precisão da timeline), DSP-04 #24 (falhas audíveis/underruns), SCH-05 #29 (ring buffer), DSP-05 #30 (polifonia/PR #94), INT-05 #31 (samples opcionais). CMP-05 #27 requer decisão sobre mini-notação na sintaxe batch antes de manter ou encerrar.

**Nota:** as tarefas não concluídas não são consideradas feitas pelo simples encerramento do período. São remanejadas para Sprint 4/5 conforme prioridade.

## 7. Sprint 04 — Validação acadêmica e P2 (15/10–04/11)

**Objetivo:** demonstrar evolução desde o P1, com o interpretador efetivamente integrado, análise semântica validada, evidências de testes e planejamento coerente.

| Prioridade | Tarefa/issue | Entrega esperada |
|---|---|---|
| P0 | DOC-04 #26 — estratégia de testes e material P2 | Evidências por fase, progresso desde P1, mudanças de escopo, limitações e formulário |
| P0 | INT-06 #37 — suíte end-to-end batch (reformulada) | Casos válidos/inválidos de `.mus` e verificação de timeline/erros |
| P0 | **DOC-07 (nova)** — gramática EBNF | Gramática formal coerente com `lexer.l`, `parser.y` e exemplos |
| P0 | **TEST-01 (nova)** — matriz de conformidade | Requisito → código → teste → resultado → issue/PR |
| P0 | **DOC-08 (nova, ou incorporada à #26)** — decisão de escopo | Registro da adoção batch e justificativa das issues retiradas |
| P1 | CMP-04 #21 — AST/aninhamento seguro (reformulada) | Testes de AST e parser; não bloqueia P2 salvo erro real |
| P1 | DSP-05 #30 / PR #94 | Revisão e testes de polifonia; somente marcar `Done` após merge |
| P1 | DSP-04 #24 / **BUG-01 (nova)** | Reproduzir/diagnosticar chiado; corrigir bloqueio de demonstração, se houver |
| P1 | INT-07 #43 | Cenários de alto BPM, se houver capacidade após entregas P0 |

**Gate P2:** passar CI/CTest no commit apresentado; executar exemplo válido e exemplo inválido; demonstrar tabela de símbolos e erro com linha/coluna; expor o que está integrado, o que está em PR e o que foi removido de escopo; revisar formulário antes de envio pelo líder. Não prometer features de Sprint 5 como concluídas.

## 8. Sprint 05 — Estabilização e ampliação controlada (05/11–18/11)

| Prioridade | Tarefa/issue | Entrega esperada |
|---|---|---|
| P1 | MAT-04 #22 e MAT-08 #45 (reformuladas) | Precisão de BPM/duração e ausência de drift em música finita |
| P1 | SCH-05 #29 e MAT-07 #40 (reformuladas) | Testes de capacidade do ring buffer e cálculos temporais usados |
| P1 | DSP-04 #24, DSP-05 #30, DSP-07 #42, DSP-08 #47, DSP-10 #57 | Correção de artefatos e estabilidade, **conforme diagnóstico** |
| P1 | INT-07 #43 e CMP-08 #44 | Testes de carga e limites da AST |
| P1 | DOC-05 #32 e INT-08 #48 | Guia de uso e versão estabilizada para avaliação |
| P1 condicionado | **CMP-14 (nova)** — condicionais | Implementação `se/entao/senao` se ratificada como meta da entrega final |
| P2 condicionado | **CMP-15 (nova)** — funções | `defina/retorna` e chamadas apenas se escopo/tempo permitirem |
| P2 | INT-05 #31, DSP-06 #36, DSP-09 #52, MAT-10 #55, DSP-11 #62 | Melhorias opcionais, não bloqueiam entrega |
| P2 | SCH-07 #41, SCH-08 #46, MAT-11 #60, SCH-11 #61 | Análise técnica complementar quando houver necessidade |

**Gate de versão estável:** fluxos principais reproduzíveis, CI verde, bugs bloqueantes corrigidos/documentados, README e exemplos atualizados; congelar escopo antes de entrega final.

## 9. Sprint 06 — Entrega final, entrevista e fechamento (19/11–30/11)

| Prioridade | Tarefa/issue | Entrega esperada |
|---|---|---|
| P0 | INT-12 #68 | Release final/RC, executável e demonstração gravada **se exigidos no plano do grupo** |
| P0 | DOC-06 #38 | Relatório final com decisões, dificuldades, soluções, issues e PRs |
| P0 | INT-11 #63 | Guia técnico/arquitetura, build CMake e execução reproduzível |
| P1 | CMP-12 #64 | Revisão de código, warnings e análise estática |
| P1 | MAT-12 #65 | Testes finais de precisão temporal da timeline batch |
| P1 | DSP-12 #67 | Auditoria de clicks/chiados e evidências de correção |
| P2 | CMP-11 #59, SCH-12 #66 | Refinamentos não bloqueantes conforme disponibilidade |

**Gate final:** verificar forma e data de entrega no Teams/Plano de Ensino; garantir acesso ao repositório conforme orientações; todos os integrantes devem saber demonstrar e justificar léxico, sintaxe, AST, semântica, interpretação, timeline e motor de áudio.

## 10. Escopo retirado do planejamento ativo

As issues a seguir devem permanecer como registro histórico após encerramento `not planned`: SCH-04 #23, SCH-06 #35, CMP-09 #49, MAT-09 #50, SCH-09 #51, INT-09 #53, CMP-10 #54, SCH-10 #56 e INT-10 #58.

**Dependem de decisão explícita para manter:** CMP-05 #27 (mini-notação), CMP-07 #39 (polirritmia `{,}`), CMP-06 #33 (dot-chaining), MAT-06 #34 (closures). Não são requisitos do guia de interpretador nem da sintaxe base atualmente implementada; não devem bloquear P2.
