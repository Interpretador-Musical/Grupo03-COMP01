---
title: Roadmap
layout: default
nav_order: 4
---

# Roadmap do Compilador Musical

Este documento acompanha o estado **integrado à `main`** e as prioridades para P2 e entrega final. O [ADR 001]({{ site.baseurl }}{% link decisao_modelo_batch.md %}) registra a escolha pelo modelo batch em 16/09/2026; o [registro de decisões e escopo]({{ site.baseurl }}{% link decisoes_escopo_final.md %}) apresenta as consequências práticas para as próximas sprints. As decisões sobre as trilhas paralelas estão registradas no [ADR 002]({{ site.baseurl }}{% link adr_002_trilhas_paralelas.md %}).

## Produto e princípio de execução

O usuário escreve um programa `.mus` **finito**. Flex e Bison produzem tokens e AST; a análise semântica valida tipos e escopos; o interpretador transforma a AST numa `Timeline` de `SoundEvent`. A Timeline é a **representação intermediária musical**, pode ser inspecionada por `--emit-timeline` e é convertida tanto na **saída-alvo final offline em WAV PCM 16-bit** (`-o` / `--saida`, `COMP-C #108` / PR `#151`) quanto para execução pela engine de áudio.

O projeto não adota padrões musicais infinitos, live coding, hot-swapping ou entrada assíncrona como requisitos de entrega. Extensões só entram se preservarem a simplicidade, os testes e o prazo da cadeia principal.

## Funcionalidades integradas

| Capacidade | Evidência e situação |
|---|---|
| Infraestrutura CMake, Flex e Bison | CMP-01 #2, CMP-02 #9: concluídas |
| Parser, AST e interpretação batch | INT-03 #19; testes sintáticos e de integração |
| Robustez da AST | CMP-04 #21 / PR #130, concluída em 03/11 como auditoria e testes de aninhamento combinado; CMP-08 #44 / PR #100 (limite de 256 níveis) |
| Análise semântica e tabela de símbolos | CMP-13 #95; `--simbolos` |
| Gramática EBNF verificada | DOC-A #106, PR #116; precedência e associatividade testadas na CMP-05 #27 / PR #131 |
| ADR do modelo batch | DOC-B #107, PR #117 |
| Timeline como IR e emissão determinística | COMP-A #104 / PR #114; COMP-B #105 / PR #113 |
| Trilhas paralelas finitas `{ , }` | CMP-07 #39, PR #128 **integrado em 02/11**; simultaneidade sem reescala |
| Repetição, aritmética, tempos e notas | MAT-04 #22, MAT-07 #40 e testes de regressão |
| Pipeline ponta a ponta | INT-06 #37, PR #115 |
| Polifonia e limitação de pico | DSP-05 #30 / PR #118; DSP-10 #57 / PR #101; voice stealing com fade-out na DSP-07 #42 / PR #133 |
| Volume master e ganhos por subgrupo | DSP-09 #52; `GainSmoother` linear de 5 ms, ganhos `Synth`/`Sample` no `AudioEngine` e flag `--volume` na CLI e WAV |
| Filtro passa-baixa BiQuad (RBJ) | DSP-11 #62, PR #159; comando `filtro <expr>` por voz (isolado em trilhas `{ , }`), filtro no Master Bus e flag `--lpf <hz>` na CLI e WAV |
| Resampling de buffers PCM | DSP-06 #36 / PR #127; API disponível, ainda não ligada ao caminho de síntese principal |
| Testes de estresse batch | INT-07 #43 / PR #126; um milhão de eventos valida compilação/fila, não reprodução integral |
| Integridade da fila SPSC | SCH-05 #29, PR #120 |
| Auditoria estática Real-Time do callback | SCH-11 #61, PR #155; `scripts/check_realtime.py` integrado ao CTest cobrindo 33 funções do caminho de áudio e wavetable `inline const` ([Auditoria Real-Time]({{ site.baseurl }}{% link auditoria-realtime.md %})) |
| Profiling de CPU e cache | SCH-07 #41 / PR #136; medição com Callgrind e Cachegrind, sem otimização especulativa ([Profiling]({{ site.baseurl }}{% link profiling.md %})) |
| Drift temporal Timeline → frames | MAT-08 #45; soma compensada no `Playhead` e regra única de segundos → frame ([Drift temporal]({{ site.baseurl }}{% link drift_temporal.md %})) |
| Otimização do IR (dobra de constantes) | COMP-D #110; passo entre semântica e interpretador, Timeline idêntica com e sem (`--sem-otimizacao`) ([Otimização]({{ site.baseurl }}{% link otimizacao.md %})) |
| Açúcar sintático no lexer | CMP-11 #59; figuras rítmicas (`colcheia`, `semínima`...) como macros de duração e `vez` = `vezes`, sem regra nova no parser ([Gramática EBNF]({{ site.baseurl }}{% link gramatica_ebnf.md %}), seção 1.5) |
| Ambientes léxicos e closures (infraestrutura) | MAT-06 #34 / PR #129; ainda sem sintaxe `defina`/`retorna` na DSL |
| Samples WAV em `toca "NOME" por d` | INT-05 #31 / PR #137, integrado em 04/11; **revisão pós-merge pendente** (ver Sprint 05) |
| Relatório e evidências do P2 | DOC-04 #26, PR #121; o formulário acadêmico deve ser verificado separadamente |
| Saída-alvo WAV PCM 16-bit | COMP-C #108, PR #151; renderização offline determinística via `-o` / `--saida` compartilhando o núcleo DSP |
| Arena allocator da AST | MAT-10 #55, PR #156; alocação em blocos contíguos vinculados à `ast::Arvore` e envenenamento ASan (`MUS_COM_ASAN=1`) |
| Envelopes adaptativos anti-clique | #123, PR #148; rampas proporcionais abaixo de 10 ms sem falso *underrun* |
| Limites numéricos e de execução | #124 e #125, PR #141; recusa de `NaN`/infinitos e tetos de eventos e iterações |
| Correção de chiados (master bus) | BUG-AUDIO #109, PR #152; `LimitadorDePico` prévio ao *soft-clipper* ([Chiados na reprodução]({{ site.baseurl }}{% link chiados.md %})) |
| Validação e empacotamento da Release M2 | INT-08 #48: base em `scripts/validar_release.sh`, workflow e [Validação da Release M2]({{ site.baseurl }}{% link release_m2.md %}) (`PR #146`); resolução portável de `samples/`, `scripts/empacotar_release.sh`, versão `0.5.0` e tag `v0.5.0-m2` em revisão no **PR `#162`** |
| Avisos estritos e Cppcheck | CMP-12 #64, PR #145; `-DMUS_WERROR=ON` no CI e análise estática em `src/` |
| Concorrência em execução com áudio | SCH-08 #46, PR #144; ThreadSanitizer no executável tocando áudio |
| Consumo ocioso de CPU | SCH-12 #66, PR #153; redução no callback sem espera ativa ([Consumo ocioso]({{ site.baseurl }}{% link consumo_ocioso.md %})) |
| Regressões matemáticas | MAT-12 #65, PR #149; cobertura de beats, BPM, frações e trilhas |
| Manual do usuário e Doxygen | DOC-05 #32, PR #154; [Manual do usuário]({{ site.baseurl }}{% link manual_usuario.md %}) e docstrings em `src/` |
| Arquitetura e guia consolidados | INT-11 #63, PR #158; unificação das 6 fases entre `README.md`, `docs/arquitetura.md` e `docs/guia_ambiente.md` |
| Relatório final e licença MIT | DOC06 #38, PR #161; [Relatório final]({{ site.baseurl }}{% link relatorio_final.md %}) e arquivo `LICENSE` na raiz |
| Release final e demonstração | INT-12 #68; [Release Final e Demonstração]({{ site.baseurl }}{% link release_final.md %}) com roteiro das 6 fases e guia para entrevista |

**Nota sobre testes:** contagens de casos e asserções mudam a cada PR. Consultar os resultados da CI no commit avaliado; não usar números históricos como se fossem totais atuais.

## P2 — Sprint 04 (encerrada em 04/11)

O P2 utiliza a gramática e o pipeline já integrados: Flex → Bison → AST → análise semântica → Timeline. Não exige nova sintaxe, reescrita da AST, closures ou saída-alvo que ainda não existe.

Todos os itens da Sprint 04 foram integrados à `main`; a CMP-04 #21 foi concluída como auditoria e testes (PR #130), sem reescrita da AST. O formulário do P2 é uma entrega à parte, enviada pelo líder. As evidências e os resultados do P2 estão documentados em [Relatório e Evidências do P2]({{ site.baseurl }}{% link relatorio_p2.md %}), complementados pelos testes automatizados, pelos registros de estresse e pelas decisões arquiteturais do projeto.

## Sprint 05 — 05/11 a 18/11

| Prioridade | Issue | Resultado esperado / Situação |
|---|---|---|
| P0 | COMP-C #108 | **Concluída (PR #151):** saída-alvo WAV PCM 16-bit offline implementada em `src/codegen/` com flags `-o` / `--saida` e suíte `saida_wav_*` |
| P1 | #124 | **Concluída (PR #141):** recusa de `NaN`, infinito e contagens/oitavas fora da faixa antes de converter para inteiro |
| P1 | #125 | **Concluída (PR #141):** tetos de `5 000 000` eventos e `10 000 000` voltas de `repita` para evitar consumo ilimitado |
| P1 | #123 | **Concluída (PR #148):** envelopes adaptativos em notas abaixo de 10 ms e correção do critério de *underrun* |
| P1 | INT-05 #31 (revisão) | Revisar o PR #137 antes das próximas tasks: o executável procura `samples/` pelo caminho do código-fonte e carrega todos os WAV em toda execução; o evento de sample sai com frequência 0 e volume do contexto, fora das invariantes da IR e sem coluna própria no CSV |
| P1 | BUG-AUDIO #109 | **Concluída (PR #152):** diagnóstico reproduzível e correção via `LimitadorDePico` prévio ao *soft-clipper* |
| P1 | MAT-08 #45 | **Concluída (PR #142):** soma compensada de Neumaier, regra única `segundosParaFrame` e medição de drift |
| P1 | INT-11 #63 (iniciar) | **Concluída (PR #158):** documentação da arquitetura, guia de ambiente e saída-alvo WAV consolidada |
| P2 | COMP-D #110 | **Concluída (PR #143):** otimização opcional por dobra de constantes sobre a AST (`--sem-otimizacao`) |
| P2 | DOC-05 #32 | **Concluída (PR #154):** manual do usuário para a sintaxe `.mus` integrada e padronização Doxygen |

INT-05 #31, SCH-07 #41, CMP-11 #59, DSP-09 #52 e DSP-11 #62, que estavam originalmente fora desta lista, acabaram implementadas; MAT-10 #55, que também estava, foi reformulada para o modelo batch em 26/11 e implementada como arena da AST (PR #156); CMP-05 #27 e CMP-06 #33 foram fechadas com testes (PRs #131 e #132), sem implementar mini-notação nem dot-chaining.

## Sprint 06 — 19/11 a 30/11

| Prioridade | Issue | Resultado esperado / Situação |
|---|---|---|
| P0 | INT-12 #68 | **Nesta entrega:** [Release Final e Demonstração]({{ site.baseurl }}{% link release_final.md %}) — versionamento, validação reproduzível, roteiro das 6 fases, saída-alvo WAV e defesa para entrevistas |
| P0 | DOC06 #38 | **Concluída (PR #161):** [Relatório final]({{ site.baseurl }}{% link relatorio_final.md %}), adequação do `README.md` e licença MIT (`LICENSE`) |
| P1 | INT-11 #63 | **Concluída (PR #158):** `README.md`, `docs/arquitetura.md`, `docs/guia_ambiente.md` e `docs/index.md` unificados |
| P1 | CMP-12 #64 | **Concluída (PR #145):** revisão estática, avisos estritos (`MUS_WERROR=ON`) e Cppcheck integrados ao CI |
| P1 | SCH-08 #46 | **Concluída (PR #144):** ThreadSanitizer também sobre o executável tocando áudio; corridas de `setMaxVoices` e da flag de sinal corrigidas ([Concorrência]({{ site.baseurl }}{% link concorrencia.md %})) |
| P1 | MAT-12 #65 | **Concluída (PR #149):** regressões matemáticas de beats, BPM, frações e trilhas |
| P1 | DSP-12 #67 | **Implementada no PR #160 (pendente de integração direta na `main`):** envelopes indexados por inteiros, proteção de fim de sample WAV e roubo duplo `< 2 ms`, `OnePoleSmoother` nas automações de LPF/ganho e suíte `test_dsp12_anti_cliques.cpp` (proteções `#109`, `#123`, `DSP-07..11` já na `main`) |
| P2 | SCH-12 #66 | **Concluída (PR #153):** laços de espera auditados (nenhuma espera ativa); custo do callback no silêncio entre eventos reduzido 42 vezes, com saída idêntica ([Consumo ocioso]({{ site.baseurl }}{% link consumo_ocioso.md %})) |

`INT-08 #48` teve a base integrada no `PR #146` e está com o pacote portável de `samples/` e versão `0.5.0` (`v0.5.0-m2`) em revisão no **PR `#162`**. Demais extensões não obrigatórias ficam subordinadas ao escopo aprovado e aos resultados de testes. A `CMP-11 #59` (`PR #150`), a `DSP-11 #62` (`PR #159`) e a `SCH-11 #61` (`PR #155`), que estavam nesta lista, foram implementadas.

## Decisões de escopo, sem equivalências incorretas

- **CMP-07:** implementada; `{ A, B }` inicia as trilhas juntas, mantém as durações em beats e avança ao fim da mais longa. Polirritmia finita não é live coding.
- **CMP-05:** mini-notação `[ ]` e multiplicadores de padrão não implementados; a issue foi fechada com testes de precedência e associatividade da AST (PR #131). Frações em `por` permitem subdivisões, mas não criam simultaneidade sozinhas.
- **CMP-06:** dot-chaining funcional não implementado nem necessário para concluir o compilador; a recusa da sintaxe é guardada por teste (PR #132).
- **MAT-06:** PR #129 integrado à `main`; a infraestrutura de ambientes léxicos e closures foi implementada, mas `defina` e `retorna` ainda não possuem suporte sintático na DSL. Não ampliar a linguagem nesta etapa.
- **MAT-10:** a justificativa de live coding saiu em 26/11. No modelo batch, a arena da AST tira da compilação a liberação nó a nó (6,5% no profiling de 03/11) e junta a posse dos nós num dono só. O ASan da MAT-11 continua valendo e passou a enxergar dentro da arena ([Arquitetura da AST]({{ site.baseurl }}{% link ast.md %}), seção 3.4).
- **Formato final:** não anunciar WAV, MIDI, MP3 ou outro como entregue antes da decisão da COMP-C e de testes reais.

## Governança

Issue concluída significa entrega efetivamente integrada ou evidência aprovada. Issue retirada do escopo deve ser encerrada como `not planned`, preservando o histórico. PR aberto e CI verde significam **em revisão**, não entrega final. Não mudar responsáveis sem combinar com a equipe.

O cronograma histórico detalhado está em [Cronograma]({{ site.baseurl }}{% link cronograma.md %}). Datas formais devem ser conferidas no Plano de Ensino e Teams.
