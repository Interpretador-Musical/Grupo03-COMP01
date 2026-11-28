---
title: Relatório Final
layout: default
parent: Documentação
nav_order: 21
---

# Relatório Final — Encerramento Acadêmico e Consolidação do Repositório

| | |
|---|---|
| **Projeto** | Compilador Musical — DSL `.mus` (Grupo 03) |
| **Disciplina** | Compiladores 1 — Turma 01 (Prof. Sérgio Freitas) — FCTE/UnB |
| **Marco** | Encerramento Acadêmico — Sprint 06 |
| **Data** | 27 de novembro de 2026 |
| **Commit de Referência (`main`)** | `b9145af` (após integração dos PRs `#151`–`#159` e `#155`) |
| **Issue de Rastreabilidade** | [DOC06 (#38)](https://github.com/Interpretador-Musical/Grupo03-COMP01/issues/38) |
| **Licença Open-Source** | [MIT License](https://github.com/Interpretador-Musical/Grupo03-COMP01/blob/main/LICENSE) (`LICENSE`) |

---

## 1. Resumo Executivo e Escopo Atendido

O **Compilador Musical** é um **compilador em lote (*batch*)** escrito em **C++17**, **GNU Flex** e **GNU Bison** que traduz programas finitos escritos em uma *Domain-Specific Language* (DSL) em português (`.mus`) para áudio sintetizado e/ou arquivo **WAV PCM 16-bit** persistente em disco.

Desde a decisão arquitetural registrada em 16/09 ([ADR 001]({{ site.baseurl }}{% link decisao_modelo_batch.md %})), todo o projeto foi conduzido sobre uma separação estrita entre a lógica da linguagem e o processamento de sinal sonoro: **o compilador nunca interpreta comandos ou aloca estruturas da linguagem durante a reprodução sonora**. Todo programa passa integralmente pelas **6 fases formais** antes da emissão da saída-alvo ou da abertura do dispositivo de áudio:

1. **Fase 1 — Análise Léxica (`lexer/lexer.l`, `src/frontend/`):** tokenização com rastreamento de linha e coluna (`YY_USER_ACTION`), acidentes musicais (`#`, `b`), oitavas, strings de *sample*, comentários e açúcar sintático de figuras rítmicas (`semibreve` a `semifusa`) e `vez` (`CMP-11` `#59`). Inspecionável via `--tokens`.
2. **Fase 2 — Análise Sintática e AST (`parser/parser.y`, `src/interpreter/ast.h`, `src/interpreter/arena.{h,cpp}`):** gramática livre de contexto LALR(1) sem conflitos (`%expect 0`, formalizada em [Gramática EBNF]({{ site.baseurl }}{% link gramatica_ebnf.md %})), com alocação contígua de nós na **Arena de memória** (`MAT-10` `#55`, integrada a `ast::Arvore`, incluindo `ComandoFiltro` da `DSP-11` `#62`) e limite determinístico de **256 níveis de profundidade** (`CMP-08` `#44`).
3. **Fase 3 — Análise Semântica (`src/semantic/`):** construção da Tabela de Símbolos, inferência estática dos tipos `numero`, `nota`, `booleano` e `string`, validação de escopos estáticos em blocos `repita` e isolamento entre trilhas paralelas (`CMP-07` `#39`), além de recusa precoce de identificadores não declarados ou operações inválidas (`CMP-13` `#95`). Inspecionável via `--simbolos`.
4. **Fase 4 — Otimização da AST (`src/otimizacao/dobra_constantes.{h,cpp}`):** dobra de constantes (`COMP-D` `#110`) sobre subexpressões com operandos constantes, compartilhando exatamente a mesma semântica aritmética de `src/interpreter/operacoes.{h,cpp}` para garantir equivalência bit a bit na `Timeline`. Ativa por padrão e desativável com `--sem-otimizacao`.
5. **Fase 5 — Geração da Representação Intermediária (`src/interpreter/`, `src/core/`):** o `Interpretador` *tree-walking* percorre a AST otimizada e, usando o `Playhead` com soma compensada de Neumaier e âncoras de andamento (`MAT-08` `#45`), gera a **`Timeline`**, uma lista linear de instruções `SoundEvent` (40 bytes cada, 7 campos incluindo `cutoffHz` no *padding* final) regida pelas 6 invariantes formais da `COMP-A` (`#104`) e protegida por tetos de execução (`5 000 000` eventos e `10 000 000` voltas de `repita`). Exportável em CSV determinístico via `--emit-timeline` (`COMP-B` `#105`).
6. **Fase 6 — Geração de Saída-Alvo Musical e Execução (`src/codegen/`, `src/audio/`):**
   - **Saída-alvo persistente (`src/codegen/gerador_wav.{h,cpp}`):** renderização offline em **WAV PCM 16-bit linear little-endian, mono, 44 100 Hz** acionada por `-o <arquivo>` / `--saida <arquivo>` (`COMP-C` `#108`), sem abrir placa de som nem repetir o *parsing*;
   - **Execução ao vivo (`src/audio/audio_engine.{h,cpp}`, `src/core/ring_buffer.h`):** fila SPSC *lock-free* pré-alocada (`carregarFila`) consumida pelo callback `miniaudio`, compartilhando exatamente o mesmo núcleo DSP (`AudioEngine::render`): síntese por wavetable (`DSP-08` `#47`), leitura de *samples* WAV (`INT-05` `#31`), envelopes adaptativos anti-clique (`#123`), filtro passa-baixa BiQuad RBJ por voz e no master bus (`filtro`/`--lpf`, `DSP-11` `#62`, `PR #159`), *voice stealing* para até 32 vozes (`DSP-07` `#42`), ganhos por subgrupo, limitador de pico + *soft-clipper* (`BUG-AUDIO` `#109`, `DSP-10` `#57`), Volume Master suavizado em 5 ms (`--volume`, `DSP-09` `#52`), callback otimizado para silêncio entre eventos (`SCH-12` `#66`) e auditoria estática Real-Time do callback com wavetable `inline const` (`SCH-11` `#61`, `PR #155`).

Mais detalhes técnicos sobre o pipeline e o formato de saída constam em [Visão de Arquitetura]({{ site.baseurl }}{% link arquitetura.md %}) e [Saída-Alvo Musical (WAV)]({{ site.baseurl }}{% link saida_alvo.md %}).

---

## 2. Evolução do Projeto entre Marcos (P1 → P2 → Entrega Final)

| Dimensão | P1 (Sprint 02) | P2 (Sprint 04 — `ab31121`) | Entrega Final (Sprint 06 — `b9145af`) |
|---|---|---|---|
| **Modelo e Linguagem** | Conflito entre *live coding* infinito e modelo finito | Modelo **Batch Finito** ([ADR 001]({{ site.baseurl }}{% link decisao_modelo_batch.md %})) e trilhas paralelas `{ , }` ([ADR 002]({{ site.baseurl }}{% link adr_002_trilhas_paralelas.md %})) | DSL estabilizada com figuras rítmicas e `vez` (`CMP-11` `#59`), comando `filtro <hz>` (`DSP-11` `#62`), *samples* WAV (`INT-05` `#31`) e limites de execução (`#124`, `#125`) |
| **Front-end e AST** | Bison inicial sem EBNF nem teto | EBNF formalizada, AST tipada em C++17 com `std::variant` e teto de 256 níveis (`CMP-08` `#44`) | **Arena de memória** para todos os nós e vetores da AST (`MAT-10` `#55`) com envenenamento ASan (`MUS_COM_ASAN=1`) |
| **Semântica e Otimização** | Sem análise semântica | Tabela de Símbolos, escopos estáticos, checagem de tipos e `--simbolos` | Semântica mantida + passagem de **dobra de constantes** (`COMP-D` `#110`, `--sem-otimizacao`) com saída IR idêntica |
| **IR (`Timeline`)** | Lista ad-hoc de eventos | `Timeline` de `SoundEvent` com 6 invariantes e `--emit-timeline` | Soma compensada de Neumaier e mapeamento único `segundosParaFrame` (`MAT-08` `#45`), `SoundEvent` de 40 bytes (`Synth`/`Sample` + `cutoffHz`) |
| **Saída-Alvo e DSP** | Síntese direta sujeita a *clipping* e *underruns* | Fila SPSC *lock-free*, *voice stealing* e *soft-clipper* (chiados em aberto na `#109`) | **Saída WAV PCM 16-bit offline** (`-o`/`--saida`, `COMP-C` `#108`), envelopes adaptativos (`#123`), filtro passa-baixa BiQuad RBJ por voz e `--lpf` (`DSP-11` `#62`), `LimitadorDePico` (`#109`), subgrupos e `--volume` (`DSP-09` `#52`), consumo ocioso reduzido 42x (`SCH-12` `#66`) |
| **Qualidade e Testes** | 194 casos de teste | 409 casos padrão (99.794 asserções) e 22 testes CTest | **517 casos na suíte padrão**; **530 casos ativos em `mus_tests --count`** (`+ 7 estresse + 4 concorrência + 2 ocioso`) e **535 com `--no-skip`**, em **49 arquivos** (`test_*.cpp`) e **39 testes CTest** (incluindo `sch11_auditoria_realtime` e `sch11_testes_verificador` com 34 casos em `scripts/test_check_realtime.py`; chegando a `524` casos padrão / `537` em `--count` em `50` arquivos com a bateria `DSP-12` do PR `#160`) |
| **Análise Estática e Sanitizers** | TSan inicial | Ubuntu, macOS, ASan e TSan | Ubuntu, macOS, Cppcheck + `-DMUS_WERROR=ON` (`CMP-12` `#64`), auditoria Real-Time (`SCH-11` `#61`), ASan/LSan/UBSan com arena e TSan também sobre o executável tocando áudio (`SCH-08` `#46`) |
| **Documentação e Licença** | Documentação inicial do site | EBNF, AST, IR, Relatório P2 | Manual do usuário + Doxygen nos headers (`DOC-05` `#32`), Arquitetura e Guia de Ambiente unificados (`INT-11` `#63`), `LICENSE` MIT e Relatório Final (`DOC06` `#38`) |

### Pendências do P2 Resolvidas nas Sprints 05 e 06

Todas as pendências técnicas que haviam sido explicitadas na seção de encerramento do [Relatório P2]({{ site.baseurl }}{% link relatorio_p2.md %}) para conclusão do compilador foram resolvidas e integradas à `main`:

- **Saída-alvo musical definitiva (`COMP-C #108`, PR `#151`):** implementada em `src/codegen/gerador_wav.{h,cpp}` com flags `-o`/`--saida`.
- **Limites numéricos e de execução (`#124` e `#125`, PR `#141`):** recusa de `NaN`/infinitos/faixas inválidas e tetos determinísticos no `Interpretador` (`5 000 000` eventos e `10 000 000` voltas).
- **Notas curtas abaixo de 10 ms (`#123`, PR `#148`):** envelopes lineares proporcionais à duração da nota e correção do critério de *underrun* em notas curtas.
- **Chiados em acordes e master bus (`BUG-AUDIO #109`, PR `#152`):** introdução do `LimitadorDePico` prévio ao *soft-clipper* (`softClipBuffer`) documentada em [Chiados na reprodução]({{ site.baseurl }}{% link chiados.md %}).

---

## 3. Matriz Completa de Rastreabilidade (Requisitos → Código → Testes → Status)

| Etapa / Requisito | Implementação Principal | Cobertura / Evidência | Status e PR na `main` |
|---|---|---|---|
| **Fase 1: Análise Léxica e Açúcar Sintático** (`CMP-01`, `CMP-11`) | `lexer/lexer.l`, `src/frontend/scanner.h`, `src/frontend/frontend.{h,cpp}` | `tests/test_lexer.cpp`, `tests/test_acucar_sintatico.cpp`, `--tokens` | **Integrado** (`PR #150` para figuras rítmicas e `vez`) |
| **Fase 2: Análise Sintática e AST em Arena** (`CMP-02`, `CMP-08`, `MAT-10`, `DOC-A`) | `parser/parser.y`, `src/interpreter/ast.h`, `src/interpreter/arena.{h,cpp}` | `tests/test_parser.cpp`, `tests/test_gramatica.cpp`, `tests/test_arena.cpp`, `tests/test_profundidade.cpp`, `tests/test_trilhas.cpp` | **Integrado** (`PR #100`, `PR #116`, `PR #128`, `PR #130`, `PR #156`) |
| **Fase 3: Análise Semântica e Escopos** (`CMP-07`, `CMP-13`) | `src/semantic/tabela_simbolos.{h,cpp}`, `src/semantic/analisador_semantico.{h,cpp}` | `tests/test_tabela_simbolos.cpp`, `tests/test_analisador_semantico.cpp`, `integracao_cli_recusa_erro_semantico`, `--simbolos` | **Integrado** (`PR #128` para isolamento em trilhas; suíte de erros em `exemplos/erros_semanticos.mus`) |
| **Fase 4: Otimização — Dobra de Constantes** (`COMP-D #110`) | `src/otimizacao/dobra_constantes.{h,cpp}`, `src/interpreter/operacoes.{h,cpp}` | `tests/test_dobra_constantes.cpp`, comparação via `--sem-otimizacao --emit-timeline` | **Integrado** (`PR #143`) |
| **Fase 5: IR (`Timeline`), Playhead e Emissão CSV** (`COMP-A`, `COMP-B`, `MAT-08`, `#124`, `#125`) | `src/interpreter/interpreter.{h,cpp}`, `src/core/playhead.{h,cpp}`, `src/core/frames.h`, `src/core/sound_event.h`, `src/core/timeline_emit.{h,cpp}` | `tests/test_representacao_intermediaria.cpp`, `tests/test_timeline_emit.cpp`, `tests/test_drift.cpp`, `tests/test_calculos.cpp`, 13 testes `emit_timeline_*` e `ir_*` no CTest | **Integrado** (`PR #113`, `PR #114`, `PR #141`, `PR #142`, `PR #149`) |
| **Fase 6a: Saída-Alvo WAV PCM 16-bit** (`COMP-C #108`) | `src/codegen/gerador_wav.{h,cpp}`, `src/main.cpp` | `tests/test_gerador_wav.cpp`, 6 testes `saida_wav_*` no CTest (`tests/saida_wav/verifica.cmake`) | **Integrado** (`PR #151`) |
| **Fase 6b: Motor de Áudio, DSP e Fila SPSC** (`SCH-05`, `DSP-05..11`, `#109`, `#123`, `INT-05`) | `src/core/ring_buffer.h`, `src/core/timeline_queue.{h,cpp}`, `src/audio/audio_engine.{h,cpp}`, `src/audio/wavetable.h`, `src/audio/biquad_filter.h`, `src/audio/limiter.h`, `src/audio/gain_smoother.h`, `src/audio/sample_bank.{h,cpp}` | `tests/test_ring_buffer.cpp`, `tests/test_timeline_queue.cpp`, `tests/test_audio_engine.cpp`, `tests/test_audio_envelope.cpp`, `tests/test_biquad_filter.cpp`, `tests/test_limiter.cpp`, `tests/test_volume_master.cpp`, `tests/test_wavetable.cpp`, `tests/test_sample_bank.cpp` | **Integrado** (`PR #118`, `PR #120`, `PR #127`, `PR #133`, `PR #137`, `PR #147`, `PR #148`, `PR #152`, `PR #157`, `PR #159`) |
| **Concorrência, Consumo Ocioso, Auditoria Real-Time, Avisos e CLI** (`INT-06`, `INT-07`, `CMP-12`, `SCH-08`, `SCH-11`, `SCH-12`) | `src/main.cpp`, `src/cli/options.{h,cpp}`, `src/cli/logger.{h,cpp}`, `src/engine/engine.{h,cpp}`, `scripts/check_realtime.py` | `tests/test_integracao.cpp`, `tests/test_concorrencia.cpp`, `tests/test_ocioso.cpp`, `tests/test_estresse.cpp`, `scripts/test_check_realtime.py` (`sch11_*`), 14 testes `integracao_cli_*` e `estresse_cli_*`, Cppcheck e `-DMUS_WERROR=ON` | **Integrado** (`PR #115`, `PR #126`, `PR #144`, `PR #145`, `PR #153`, `PR #155`) |
| **Documentação, Manual, Release M2 e Repositório** (`DOC-04`, `DOC-05`, `INT-08`, `INT-11`, `DOC06`) | `README.md`, `LICENSE`, `docs/*.md`, headers `src/**/*.h`, `scripts/validar_release.sh` | Site GitHub Pages (`docs/_config.yml`), Doxygen nos headers, workflow `release-validation.yml` | **Integrado / Esta entrega** (`PR #121`, `PR #146`, `PR #154`, `PR #158`, `DOC06 #38`) |

---

## 4. Evidências de Validação Automatizada e Qualidade (`b9145af`)

### 4.1 Cobertura de Testes Automatizados (`CTest` e `doctest`)

No commit de referência `b9145af` da `main`, o projeto conta com **49 arquivos de teste C++** (`tests/test_*.cpp`), a suíte Python do verificador Real-Time (`scripts/test_check_realtime.py`, 34 casos) e **39 alvos registrados no CTest** (`ctest --test-dir build -N`), cobrindo:

- **4 suítes binárias `doctest`:** `mus_tests` (suíte padrão), `mus_tests_estresse` (7 casos), `mus_tests_concorrencia` (4 casos) e `mus_tests_ocioso` (2 casos);
- **5 testes de emissão da IR (`emit_timeline_*`):** comparação byte a byte para `escala`, `arpejo` e `polirritmia` além da recusa com código de saída 1 para erros sintáticos e semânticos;
- **8 testes de invariantes da IR (`ir_*`):** notas/pausas, andamento, repetição, expressões, trilhas e recusa de volume, `repita` infinito e oitava inválida;
- **10 testes de integração da CLI (`integracao_cli_*`):** execução `--no-audio` em `escala`, `arpejo` e `polirritmia`, volume master válido/inválido (`--volume`), filtro passa-baixa válido/inválido (`--lpf`) e recusa de arquivo inexistente e erros;
- **4 testes de estresse da CLI (`estresse_cli_*`):** 1500 BPM denso, carga mista, 1 milhão de eventos sem áudio e recusa acima do teto;
- **6 testes de geração WAV (`saida_wav_*`):** cabeçalho RIFF/WAVE e equivalência com `--sem-otimizacao` em `escala`, `arpejo` e `polirritmia`, além de recusa de erro sintático, erro semântico e caminho inválido;
- **2 testes de auditoria Real-Time (`sch11_*`):** verificação estática das 33 funções do caminho do callback (`sch11_auditoria_realtime`) e suíte de 34 testes unitários do verificador (`sch11_testes_verificador`).

Execução observada no CI do commit `b9145af` ([CI Workflow Run #46080311151](https://github.com/Interpretador-Musical/Grupo03-COMP01/actions/runs/46080311151)) para o binário completo `./build/tests/mus_tests` (sob ThreadSanitizer):

```text
[doctest] doctest version is "2.4.11"
[doctest] run with "--help" for options
===============================================================================
[doctest] test cases:    530 |    530 passed | 0 failed | 5 skipped
[doctest] assertions: 202564 | 202564 passed | 0 failed |
[doctest] Status: SUCCESS!
```

Composição exata dos casos em `mus_tests`: `517` casos na suíte padrão `+ 7` (`estresse`) `+ 4` (`concorrencia`) `+ 2` (`ocioso`) `= 530` casos ativos reportados por `./build/tests/mus_tests --count`, somados a `5` casos de medição/benchmark marcados com `doctest::skip()` (`tests/test_perfil.cpp`, `tests/test_dobra_constantes.cpp` e `tests/test_drift.cpp`), totalizando `535` casos com `--no-skip` (e `531` casos ativos / `202.567` asserções sob `ENABLE_ASAN=ON`, que inclui a checagem de envenenamento da arena). No `ctest --test-dir build --output-on-failure`, todos os **39/39 testes** passaram em `ubuntu-latest` e `macos-latest`.

### 4.2 Análise Estática, Sanitizers e Integração Contínua

Todo pull request e push na `main` passa pelos workflows em `.github/workflows/ci.yml` e `.github/workflows/release-validation.yml`:

- **Compilação estrita multiplataforma (`MUS_WERROR=ON`):** Ubuntu (GCC) e macOS (Clang + Flex/Bison 2.3 do Xcode Command Line Tools).
- **Análise estática (`Cppcheck`):** verificação sobre `src/` com `--std=c++17 --error-exitcode=1` (justificativa das flags em [Análise estática]({{ site.baseurl }}{% link analise_estatica.md %})).
- **Auditoria estática Real-Time (`SCH-11` `#61`):** `scripts/check_realtime.py` verifica 33 funções/métodos do callback em 11 arquivos contra alocações, containers dinâmicos, I/O, locks, `throw` e `static` locais sem `constexpr`.
- **Memória (`ASan` + `LSan` + `UBSan`):** suíte completa executada com `-DENABLE_ASAN=ON` (`MUS_COM_ASAN=1` ativa envenenamento dos blocos livres na `Arena` da AST).
- **Concorrência (`TSan`):** suíte de testes (`mus_tests`) e executável `compilador` tocando os arquivos `.mus`, `--test-tone` e `--loop` sob `TSAN_OPTIONS=halt_on_error=1` (`SCH-08` `#46`).
- **Execuções verificadas na `main` (`b9145af`):**
  - [CI Workflow Run #46080311151](https://github.com/Interpretador-Musical/Grupo03-COMP01/actions/runs/46080311151) — **concluído com sucesso** (`push` na `main` em `b9145af`: 39/39 CTest, Cppcheck, ASan e TSan);
  - [Pages Build and Deployment #46080310085](https://github.com/Interpretador-Musical/Grupo03-COMP01/actions/runs/46080310085) — **concluído com sucesso** (`b9145af`).
- **Execuções de `Release Validation` em `pull_request` (`release-validation.yml` não dispara em `push` na `main`):**
  - [Release Validation #46050167135](https://github.com/Interpretador-Musical/Grupo03-COMP01/actions/runs/46050167135) — **concluído com sucesso** em `pull_request` (PR `#161`, commit `b32060f` sobre a base `2679ec4`);
  - [Release Validation #45916177080](https://github.com/Interpretador-Musical/Grupo03-COMP01/actions/runs/45916177080) — **concluído com sucesso** em `pull_request` (PR `#158`, commit `cfcb601`).

---

## 5. Limitações Técnicas Conhecidas e Decisões de Escopo

Em conformidade com as diretrizes de transparência documental ([Diretrizes de Escopo]({{ site.baseurl }}{% link decisoes_escopo_final.md %})), registram-se abaixo as limitações técnicas reais do compilador e o estado objetivo das issues restantes no fechamento desta entrega:

### 5.1 Limitações Técnicas Reais

1. **Localização e carregamento inicial da pasta `samples/`:** conforme registrado em [Validação da Release M2]({{ site.baseurl }}{% link release_m2.md %}) (seção 6.1), a execução de `compilador --no-audio exemplos/escala.mus` (carregando os 85 *samples* WAV) levou **`13,98 s`** no WSL com o repositório em `/mnt/c/`, contra aproximadamente **`0,15 s`** em um Mac com build Release — **a diferença sugere influência significativa do sistema de arquivos ou do ambiente WSL, mas a causa não foi isolada**. Além disso, no commit `b9145af` da `main` a localização padrão dos WAVs utiliza o caminho `MUS_SOURCE_DIR` definido na compilação; a resolução portável relativa ao executável (com busca relativa ao executável e `MUS_SAMPLES_DIR`) e o pacote distribuível da versão `0.5.0` estão sendo introduzidos no **PR `#162`** (`INT-08 #48`, aberto em revisão).
2. **Representação de *samples* no CSV de `--emit-timeline`:** no formato CSV atual (5 colunas), eventos `Sample` emitidos por `toca "NOME" por d` aparecem com `frequencia_hz = 0.000000` e volume do contexto; o campo `sampleId` existe apenas na estrutura `SoundEvent` em memória e é usado na síntese ao vivo e na renderização WAV (`-o`/`--saida`).
3. **Condicionais (`se`/`entao`/`senao`) e funções (`defina`/`retorna`):** os tokens existem no lexer e a infraestrutura de `Ambiente` e `Closure` existe em C++ (`MAT-06` `#34`, PR `#129`), mas as produções permanecem fora de `parser/parser.y` por decisão explícita de escopo (Sprint 04/05), priorizando a estabilidade e a verificação estática das estruturas iterativas e paralelas do modelo batch.
4. **Mini-notação e encadeamento funcional:** a notação `[ ]` e operadores de padrão do TidalCycles (`CMP-05` `#27`, PR `#131`) e *dot-chaining* (`CMP-06` `#33`, PR `#132`) não foram implementados; a suíte automatizada assegura recusa explícita dessas sintaxes sem ambiguidade gramatical.
5. **Reprodução em áudio sem hardware (`--no-audio` e `-o`):** em ambientes sem placa de som física (como servidores de CI), a reprodução ao vivo utiliza o backend nulo ou exige `--no-audio`, enquanto a flag `-o`/`--saida` gera o arquivo `.wav` de modo totalmente portátil e independente de hardware.

### 5.2 Situação das Demais Issues no Commit `b9145af`

| Issue | Título / Escopo | Situação no Commit `b9145af` |
|---|---|---|
| `DSP-11 #62` | Filtro Passa-Baixa (`BiQuad LPF`) | **Integrada na `main` (PR `#159`):** comando `filtro <expr>` por voz (isolado em trilhas `{ , }`), `SoundEvent::cutoffHz`, `BiquadFilter` RBJ no motor e flag `--lpf <hz>` na CLI e geração WAV. |
| `SCH-11 #61` | Hook para restrição Real-Time estrita | **Integrada na `main` (PR `#155`):** verificador estático `scripts/check_realtime.py` cobrindo 33 funções do caminho de áudio, suíte de 34 testes unitários (`scripts/test_check_realtime.py`), alvos `sch11_*` no CTest e wavetable `inline const`. |
| `DSP-12 #67` | Bateria final de análise anti-falhas e cliques | Implementada no **PR `#160`** (envelopes indexados por inteiros, proteção no fim físico de *samples* WAV e roubo duplo `< 2 ms`, `OnePoleSmoother` nas automações de LPF/ganho e suíte `test_dsp12_anti_cliques.cpp`; já aprovada e mesclada ao ramo do `#159`, pendente de integração direta na `main`). |
| `INT-08 #48` | Validação e Release M2 (Beta 0.5) | Infraestrutura base (`scripts/validar_release.sh`, `.github/workflows/release-validation.yml` e [Validação da Release M2]({{ site.baseurl }}{% link release_m2.md %})) integrada na `main` (`PR #146`); a resolução portável de `samples/` sem depender de `MUS_SOURCE_DIR`, o empacotamento (`scripts/empacotar_release.sh`), a versão `0.5.0` e a tag `v0.5.0-m2` estão abertos em revisão no **PR `#162`**. |
| `INT-12 #68` | Release final e demonstração | Etapa subsequente à consolidação do repositório (`DOC06 #38`), responsável pela tag final e pelo roteiro de demonstração e entrevista da disciplina. |

---

## 6. Reprodução Completa das 6 Fases

Comandos verificados na raiz do repositório (Linux, macOS ou WSL):

```bash
# Build com avisos tratados como erro
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DMUS_WERROR=ON
cmake --build build --parallel

# Todas as fases do compilador sobre o exemplo canônico
./build/compilador --tokens exemplos/arpejo.mus                         # Fase 1: Léxica
./build/compilador --simbolos exemplos/arpejo.mus                       # Fase 3: Semântica (após Fase 2: AST)
./build/compilador -q --sem-otimizacao --emit-timeline exemplos/arpejo.mus # Fase 4 -> 5: IR sem dobra
./build/compilador -q --emit-timeline exemplos/arpejo.mus               # Fase 5: IR determinística (CSV)
./build/compilador -o arpejo.wav exemplos/arpejo.mus                    # Fase 6: Saída-alvo WAV PCM 16-bit
./build/compilador --no-audio exemplos/polirritmia.mus                  # Pipeline completo sem dispositivo de som

# Validação da suíte completa (39 testes) e release
ctest --test-dir build --output-on-failure
bash scripts/validar_release.sh
```
