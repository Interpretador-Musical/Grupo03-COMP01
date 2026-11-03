---
title: Roadmap
layout: default
nav_order: 4
---

# Roadmap do Compilador Musical

Este documento acompanha o estado **integrado à `main`** e as prioridades para P2 e entrega final. O [ADR 001]({{ site.baseurl }}{% link decisao_modelo_batch.md %}) registra a escolha pelo modelo batch em 16/09/2026; o [registro de decisões e escopo]({{ site.baseurl }}{% link decisoes_escopo_final.md %}) apresenta as consequências práticas para as próximas sprints. As decisões sobre as trilhas paralelas estão registradas no [ADR 002]({{ site.baseurl }}{% link adr_002_trilhas_paralelas.md %}).

## Produto e princípio de execução

O usuário escreve um programa `.mus` **finito**. Flex e Bison produzem tokens e AST; a análise semântica valida tipos e escopos; o interpretador transforma a AST numa `Timeline` de `SoundEvent`. A Timeline é a **representação intermediária musical**, pode ser inspecionada por `--emit-timeline` e é consumida pela engine de áudio. A definição de uma saída-alvo final independente permanece na COMP-C (#108).

O projeto não adota padrões musicais infinitos, live coding, hot-swapping ou entrada assíncrona como requisitos de entrega. Extensões só entram se preservarem a simplicidade, os testes e o prazo da cadeia principal.

## Funcionalidades integradas

| Capacidade | Evidência e situação |
|---|---|
| Infraestrutura CMake, Flex e Bison | CMP-01 #2, CMP-02 #9: concluídas |
| Parser, AST e interpretação batch | INT-03 #19; testes sintáticos e de integração |
| Análise semântica e tabela de símbolos | CMP-13 #95; `--simbolos` |
| Gramática EBNF verificada | DOC-A #106, PR #116 |
| ADR do modelo batch | DOC-B #107, PR #117 |
| Timeline como IR e emissão determinística | COMP-A #104 / PR #114; COMP-B #105 / PR #113 |
| Trilhas paralelas finitas `{ , }` | CMP-07 #39, PR #128 **integrado em 02/11**; simultaneidade sem reescala |
| Repetição, aritmética, tempos e notas | MAT-04 #22, MAT-07 #40 e testes de regressão |
| Pipeline ponta a ponta | INT-06 #37, PR #115 |
| Polifonia e limitação de pico | DSP-05 #30 / PR #118; DSP-10 #57 / PR #101 |
| Resampling de buffers PCM | DSP-06 #36 / PR #127; API disponível, ainda não ligada ao caminho de síntese principal |
| Testes de estresse batch | INT-07 #43 / PR #126; um milhão de eventos valida compilação/fila, não reprodução integral |
| Integridade da fila SPSC | SCH-05 #29, PR #120 |
| Relatório e evidências do P2 | DOC-04 #26, PR #121; o formulário acadêmico deve ser verificado separadamente |

**Nota sobre testes:** contagens de casos e asserções mudam a cada PR. Consultar os resultados da CI no commit avaliado; não usar números históricos como se fossem totais atuais.

## P2 — Sprint 04 (até 04/11)

O P2 utiliza a gramática e o pipeline já integrados: Flex → Bison → AST → análise semântica → Timeline. Não exige nova sintaxe, reescrita da AST, closures ou saída-alvo que ainda não existe.

As prioridades restantes são: comprovar os exemplos e a CI, conferir os dados do relatório, distinguir P2 técnico de submissão formal e verificar se a CMP-04 #21 possui critérios não satisfeitos. As evidências e os resultados do P2 estão documentados em [Relatório e Evidências do P2]({{ site.baseurl }}{% link relatorio_p2.md %}), complementados pelos testes automatizados, pelos registros de estresse e pelas decisões arquiteturais do projeto.

## Sprint 05 — 05/11 a 18/11

| Prioridade | Issue | Resultado esperado |
|---|---|---|
| P0 | COMP-C #108 | Escolher **um** formato-alvo musical, justificar e implementar transformação da Timeline com testes |
| P1 | #124 | Recusar `NaN`, infinito e contagens/oitavas fora da faixa antes de converter para inteiro |
| P1 | #125 | Definir limite de eventos e iterações para evitar consumo ilimitado de memória/CPU |
| P1 | #123 | Documentar/estabilizar notas abaixo de 10 ms e contagem correta de underruns |
| P1 | BUG-AUDIO #109 | Diagnóstico reproduzível e correção das falhas verificadas |
| P1 | MAT-08 #45 | Medir drift entre Timeline e frames renderizados |
| P1 | INT-11 #63 (iniciar) | Preparar documentação da arquitetura e do formato final escolhido |
| P2 | COMP-D #110 | Otimização opcional, apenas após estabilizar a saída-alvo |
| P2 | DOC-05 #32 | Manual de uso para a sintaxe efetivamente integrada |

Recursos adicionais de sintaxe, samples nomeados, profiling por otimização especulativa e filtros não serão implementados nesta entrega. Recomenda-se encerrar como `not planned` INT-05 #31, CMP-05 #27, CMP-06 #33, SCH-07 #41, DSP-09 #52, MAT-10 #55, CMP-11 #59 e DSP-11 #62, preservando o histórico e os responsáveis no registro do GitHub.

## Sprint 06 — 19/11 a 30/11

| Prioridade | Issue | Resultado esperado |
|---|---|---|
| P0 | INT-12 #68 | Release reproduzível e demonstração |
| P0 | DOC06 #38 | Relatório final e rastreabilidade acadêmica |
| P1 | INT-11 #63 | README, arquitetura e instruções da saída-alvo já implementada |
| P1 | CMP-12 #64 | Revisão estática, warnings e regressões |
| P1 | MAT-12 #65 | Regressões matemáticas de beats, BPM, frações e trilhas |
| P1 | DSP-12 #67 | Verificação de áudio e regressões das falhas corrigidas |
| P2 | SCH-12 #66 | Medição pontual de CPU do callback, se houver necessidade demonstrada |

SCH-11 #61, DSP-11 #62, CMP-11 #59 e demais extensões não obrigatórias ficam subordinadas ao escopo aprovado e aos resultados de testes.

## Decisões de escopo, sem equivalências incorretas

- **CMP-07:** implementada; `{ A, B }` inicia as trilhas juntas, mantém as durações em beats e avança ao fim da mais longa. Polirritmia finita não é live coding.
- **CMP-05:** mini-notação `[ ]` e multiplicadores de padrão não implementados. Frações em `por` permitem subdivisões, mas não criam simultaneidade sozinhas.
- **CMP-06:** dot-chaining funcional não implementado nem necessário para concluir o compilador.
- **MAT-06:** PR #129 integrado à `main`; a infraestrutura de ambientes léxicos e closures foi implementada, mas `defina` e `retorna` ainda não possuem suporte sintático na DSL. Não ampliar a linguagem nesta etapa.
- **MAT-10:** ASan (MAT-11) detecta erros de memória; não substitui tecnicamente um allocator. A justificativa de live coding da issue original deve ser retirada ou a issue encerrada por decisão explícita.
- **Formato final:** não anunciar WAV, MIDI, MP3 ou outro como entregue antes da decisão da COMP-C e de testes reais.

## Governança

Issue concluída significa entrega efetivamente integrada ou evidência aprovada. Issue retirada do escopo deve ser encerrada como `not planned`, preservando o histórico. PR aberto e CI verde significam **em revisão**, não entrega final. Não mudar responsáveis sem combinar com a equipe.

O cronograma histórico detalhado está em [Cronograma]({{ site.baseurl }}{% link cronograma.md %}). Datas formais devem ser conferidas no Plano de Ensino e Teams.
