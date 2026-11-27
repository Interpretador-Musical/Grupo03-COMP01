---
title: Release Final e Demonstração
layout: default
parent: Documentação
nav_order: 22
---

# INT-12 — Release Final e Demonstração do Compilador Musical

| | |
|---|---|
| **Projeto** | Compilador Musical — DSL `.mus` (Grupo 03) |
| **Disciplina** | Compiladores 1 — Turma 01 (Prof. Sérgio Freitas) — FCTE/UnB |
| **Issue de Rastreabilidade** | [INT-12 (#68)](https://github.com/Interpretador-Musical/Grupo03-COMP01/issues/68) |
| **Commit de Referência (`main`)** | `2d8eac6` (após integração dos PRs `#151`–`#159` e `#161`) |
| **Dependências Integradas** | `COMP-C` (`#108`), `INT-11` (`#63`), `DOC-04` (`#26`), `DOC06` (`#38`) |
| **Histórico de Tags** | `v0.1.0-m1` (Release M1) · Beta 0.5 (`INT-08`, `#146` integrado e `#162` em revisão) · Release Final (Sprint 06) |
| **Licença** | [MIT License](https://github.com/Interpretador-Musical/Grupo03-COMP01/blob/main/LICENSE) (`LICENSE`) |

---

## 1. Política de Versionamento, Empacotamento e Reprodutibilidade

O **Compilador Musical** adota compilação reproduzível a partir do código-fonte via **CMake (C++17)**, **GNU Flex** e **GNU Bison**, com entrega automatizada validada pelo script `scripts/validar_release.sh` e pelos workflows `.github/workflows/ci.yml` e `.github/workflows/release-validation.yml`.

- **Versionamento:**
  - `v0.1.0-m1` (`INT-04`, PR `#97`): baseline funcional do P1/P2 ([Notas da Release M1]({{ site.baseurl }}{% link release_m1.md %})).
  - Beta 0.5 / M2 (`INT-08 #48`): infraestrutura de validação em `scripts/validar_release.sh` e [Validação da Release M2]({{ site.baseurl }}{% link release_m2.md %}) já integrada na `main` (`PR #146`), com a resolução portável da pasta `samples/`, o empacotamento (`scripts/empacotar_release.sh`), a versão `0.5.0` e a tag `v0.5.0-m2` em revisão no **PR `#162`**.
  - **Release Final (Sprint 06, `INT-12 #68`):** consolidação completa das **6 fases do compilador**, saída-alvo persistente em **WAV PCM 16-bit** (`COMP-C`), documentação final (`INT-11`, `DOC06`) e roteiro de demonstração reproduzível para avaliação e entrevistas. A publicação da tag final no GitHub ocorre após a integração dos PRs remanescentes aprovados pela equipe.
- **Reprodutibilidade a partir da fonte:**
  - Os artefatos intermediários do compilador (`--tokens`, `--simbolos`, `--emit-timeline`) são determinísticos em Linux, macOS e WSL; o CSV de `--emit-timeline` usa locale `"C"` (`%.6f`) e a suíte no CTest compara as saídas esperadas em `tests/emit_timeline/` byte a byte.
  - A geração da saída-alvo offline (`-o <arquivo>` / `--saida <arquivo>`) produz arquivo **WAV PCM 16-bit (44 100 Hz, mono)** sem depender de dispositivo de som físico e reutiliza exatamente `AudioEngine::render`, garantindo equivalência entre a geração offline e a reprodução ao vivo.
- **Empacotamento e localização de `samples/`:**
  - No commit `2d8eac6` da `main`, o caminho padrão dos 85 arquivos `.wav` da pasta `samples/` é definido em tempo de compilação via `MUS_SOURCE_DIR`; portanto, ao executar o compilador a partir da árvore clonada, todos os exemplos e testes localizam `samples/` automaticamente.
  - A busca independente do diretório de build (via `MUS_SAMPLES_DIR` e caminho relativo ao executável, sem depender de `MUS_SOURCE_DIR`) junto com o empacotamento em `.tar.gz` com `SHA-256` estão em revisão no **PR `#162`**.

---

## 2. Verificação em Ambiente Limpo (Instalação, Build e Testes)

Todos os comandos abaixo foram conferidos segundo o [Guia de Ambiente e Execução]({{ site.baseurl }}{% link guia_ambiente.md %}):

### 2.1 Dependências e Compilação Limpa

**Ubuntu / Debian e Windows (via WSL — clone recomendado em `~/Grupo03-COMP01`, fora de `/mnt/c/`):**

```bash
sudo apt update
sudo apt install flex bison build-essential cmake git libasound2-dev libpulse-dev -y

git clone https://github.com/Interpretador-Musical/Grupo03-COMP01.git
cd Grupo03-COMP01

# Validação completa de Release com avisos como erro (-DMUS_WERROR=ON) e 39 testes CTest
bash scripts/validar_release.sh
```

**macOS (Xcode Command Line Tools com Flex + Bison 2.3 compatível com `parser/parser.y`, mais CMake via Homebrew):**

```bash
xcode-select --install
brew install cmake

git clone https://github.com/Interpretador-Musical/Grupo03-COMP01.git
cd Grupo03-COMP01
bash scripts/validar_release.sh
```

### 2.2 Resultados Verificados na `main`

No snapshot integrado da `main` (`2d8eac6` / código C++ `b9145af`), os workflows de CI passaram integralmente em `ubuntu-latest` e `macos-latest` ([CI #46083192729](https://github.com/Interpretador-Musical/Grupo03-COMP01/actions/runs/46083192729) e [CI #46080311151](https://github.com/Interpretador-Musical/Grupo03-COMP01/actions/runs/46080311151)):

- **CTest:** **39/39** alvos aprovados (`mus_tests`, `mus_tests_estresse`, `mus_tests_concorrencia`, `mus_tests_ocioso`, `emit_timeline_*`, `ir_*`, `integracao_cli_*`, `estresse_cli_*`, `saida_wav_*` e `sch11_*`).
- **Suíte unitária e de estresse (`doctest`):** **517** casos na suíte padrão, **530** casos ativos em `mus_tests --count` (**202.564** asserções sob ThreadSanitizer; **531** casos e **202.567** asserções sob AddressSanitizer com envenenamento da `Arena` em `MUS_COM_ASAN=1`) e **535** com `--no-skip`, distribuídos em **49** arquivos `test_*.cpp`.
- **Auditoria estática e Real-Time:** Cppcheck sem achados (`src/`) e `scripts/check_realtime.py` com **0 violações** nas 33 funções críticas do callback de áudio (+ **34 testes unitários** em `scripts/test_check_realtime.py`).
- **Relatório completo:** todas as matrizes de evidências e medições detalhadas estão em [Relatório Final]({{ site.baseurl }}{% link relatorio_final.md %}).

---

## 3. Roteiro de Demonstração (Todas as 6 Fases do Compilador)

Para a demonstração na entrevista ou avaliação prática, compile o binário em Debug/Release na raiz do repositório (`cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DMUS_WERROR=ON && cmake --build build --parallel`) e siga os passos abaixo em ordem:

### 3.1 Código-fonte de entrada (`exemplos/arpejo.mus`)

```mus
// Arpejo de Dó maior — quatro compassos.
//
// O playhead avança a cada nota, então o mesmo trecho repetido quatro vezes
// vira quatro compassos de música, e não quatro notas simultâneas.

andamento 120
volume 0.8

tempo = 0.5        // duração em tempos: 0.5 é uma colcheia

repita 4 vezes {
    toca do4  por tempo
    toca mi4  por tempo
    toca sol4 por tempo
}

/* Fecha uma oitava abaixo, com o dobro da duração.
   A altura é uma expressão: lá 4 menos 2 semitons dá sol 4. */
toca la4 - 2 por tempo * 2

pausa por 1.0
```

### 3.2 Fase 1 — Análise Léxica (`--tokens`)

```bash
./build/compilador --tokens exemplos/arpejo.mus
```

Mostra os **35 tokens** com `linha`, `coluna`, `token`, `lexema` e `valor` (`ANDAMENTO`, `INTEIRO 120`, `VOLUME`, `REAL 0.8`, `IDENT tempo`, etc.). Também é possível demonstrar o açúcar sintático léxico de figuras rítmicas (`colcheia`, `semínima`) e `vez` (`CMP-11 #59`).

### 3.3 Fase 2 — Análise Sintática e AST em Arena

Na chamada normal do compilador, `parser/parser.y` valida a gramática LALR(1) sem conflitos (`%expect 0`) e constrói a `ast::Arvore` alocando nós e vetores na `Arena` de blocos contíguos (`MAT-10 #55`), respeitando o limite determinístico de **256 níveis de profundidade** (`CMP-08 #44`).

### 3.4 Fase 3 — Análise Semântica e Tabela de Símbolos (`--simbolos`)

```bash
./build/compilador --simbolos exemplos/arpejo.mus
```

Saída esperada:

```text
  endereço  nome             tipo      escopo  declarada  usos (linhas)
  --------  ---------------  --------  ------  ---------  -------------
         0  tempo            numero         0     9:1     12, 13, 14, 19

1 símbolos
```

### 3.5 Fase 4 — Otimização da AST (`COMP-D #110`, `--sem-otimizacao`)

Mostra que a dobra de constantes avalia subexpressões constantes na AST mantendo a IR rigorosamente idêntica bit a bit:

```bash
diff -u \
  <(./build/compilador -q --emit-timeline exemplos/arpejo.mus) \
  <(./build/compilador -q --sem-otimizacao --emit-timeline exemplos/arpejo.mus)
```

Sem saída no `diff` e código de retorno `0`: a `Timeline` gerada com e sem otimização é byte a byte idêntica.

### 3.6 Fase 5 — Representação Intermediária (`Timeline` e `--emit-timeline`)

```bash
./build/compilador -q --emit-timeline exemplos/arpejo.mus
```

Emite os **14 eventos** `SoundEvent` em CSV determinístico (`12` notas do arpejo, fechamento com `la4 - 2` avaliado em `sol4 = 391.995436 Hz` e `tempo * 2` = `0.500000 s` a 120 BPM, mais a `pausa` final com frequência e volume `0.000000`):

```csv
indice,inicio_s,duracao_s,frequencia_hz,volume
1,0.000000,0.250000,261.625565,0.800000
2,0.250000,0.250000,329.627557,0.800000
3,0.500000,0.250000,391.995436,0.800000
...
13,3.000000,0.500000,391.995436,0.800000
14,3.500000,0.500000,0.000000,0.000000
```

### 3.7 Fase 6 — Saída-Alvo Musical em WAV PCM 16-bit (`-o` / `--saida`) e Execução

1. **Geração offline do artefato de saída-alvo (`COMP-C #108`):**
   ```bash
   ./build/compilador -o arpejo.wav exemplos/arpejo.mus
   ```
   Saída esperada:
   ```text
   [INFO ] arquivo de entrada: exemplos/arpejo.mus
   [INFO ] 85 samples WAV carregados
   [INFO ] 14 eventos, 4 s de música
   [INFO ] saída WAV gerada: arpejo.wav (176400 frames, 4 s, 352844 bytes)
   ```
   O arquivo `arpejo.wav` segue o formato **RIFF/WAVE PCM 16-bit linear little-endian, mono, 44 100 Hz** (cabeçalho de 44 bytes + `176 400 × 2` bytes de amostras) e pode ser reproduzido em qualquer player (`aplay arpejo.wav`, `afplay arpejo.wav` ou `Media.SoundPlayer`) sem reexecutar lexer, parser ou interpretador.
2. **Controle de Volume Master (`--volume`, `DSP-09`) e Filtro Passa-Baixa BiQuad (`--lpf`, `DSP-11`):**
   ```bash
   ./build/compilador --saida arpejo_lpf.wav --volume 0.5 --lpf 1200 exemplos/arpejo.mus
   ```
3. **Execução sem placa de som (`--no-audio`) e ao vivo:**
   ```bash
   ./build/compilador --no-audio exemplos/arpejo.mus
   ./build/compilador exemplos/arpejo.mus
   ```

### 3.8 Demonstração Complementar: Trilhas Paralelas (`exemplos/polirritmia.mus`) e Erros (`exemplos/erros_semanticos.mus`)

- **Polirritmia de bloco (`CMP-07 #39` / [ADR 002]({{ site.baseurl }}{% link adr_002_trilhas_paralelas.md %})):**
  ```bash
  ./build/compilador -q --emit-timeline exemplos/polirritmia.mus
  ./build/compilador -o polirritmia.wav exemplos/polirritmia.mus
  ```
  Em `90 BPM`, gera **22 eventos** em **6 s** (`repita` com três contra quatro, acorde de três trilhas com `inicio_s` idêntico em `1.333333 s` e isolamento de `oitava` e `volume` por trilha).
- **Recusa semântica antes da IR e do áudio (`exemplos/erros_semanticos.mus`):**
  ```bash
  ./build/compilador exemplos/erros_semanticos.mus
  ```
  Reporta todos os 5 erros semânticos (variável fora de escopo do `repita`, redeclaração com troca de tipo, duração não numérica, soma inválida `nota + nota` e variável não declarada) com linha e coluna exatas e código de saída `1`, sem emitir IR nem gerar áudio.

---

## 4. Síntese de Decisões Técnicas e Limitações Reais (Para Arguição)

| Tema | Decisão / Limitação | Referência |
|---|---|---|
| **Modelo Batch Finito vs. Live Coding** | Programa `.mus` é compilado por completo antes do som (como Alda), abandonando funções infinitas do TidalCycles para permitir análise estática completa e fila pré-carregada. | [ADR 001]({{ site.baseurl }}{% link decisao_modelo_batch.md %}) |
| **Polirritmia de Bloco `{ , }`** | Trilhas paralelas partem do mesmo cursor de entrada e preservam duração em *beats* sem reescala; escopos, `oitava`, `volume` e `filtro` são locais à trilha e `andamento` é proibido dentro do grupo. | [ADR 002]({{ site.baseurl }}{% link adr_002_trilhas_paralelas.md %}) |
| **Saída-Alvo WAV PCM 16-bit Offline** | Escolhido em detrimento de MIDI (que não representa microtons/transposições contínuas nem *samples* arbitrários) e de serialização textual de eventos; reutiliza exatamente `AudioEngine::render`. | [Saída-Alvo WAV]({{ site.baseurl }}{% link saida_alvo.md %}) |
| **Gerenciamento da AST e Segurança** | Todos os nós e vetores vivem numa `Arena` ligada a `ast::Arvore` (`MAT-10`), eliminando liberação nó a nó e suportando envenenamento ASan (`MUS_COM_ASAN=1`); recursão travada em 256 níveis (`CMP-08`). | [Arquitetura da AST]({{ site.baseurl }}{% link ast.md %}) |
| **Precisão Temporal** | Acúmulo de *beats* no `Playhead` com soma compensada de Neumaier + âncoras de andamento e regra única `segundosParaFrame` para evitar *drift* e lacunas entre notas. | [Drift Temporal]({{ site.baseurl }}{% link drift_temporal.md %}) |
| **Áudio em Tempo Real Lock-Free** | Fila SPSC `SpscRingBuffer` pré-alocada (`carregarFila`), polifonia de até 32 vozes com *voice stealing*, limitador de pico + *soft-clipper* e auditoria estática Real-Time de 33 funções (`SCH-11`). | [Auditoria Real-Time]({{ site.baseurl }}{% link auditoria-realtime.md %}) |
| **Limitação — Pasta `samples/` e I/O Inicial** | Na `main` (`2d8eac6`), o compilador localiza os 85 WAVs via `MUS_SOURCE_DIR`; `compilador --no-audio exemplos/escala.mus` levou `13,98 s` no WSL (`/mnt/c/`) vs. ~`0,15 s` no Mac em Release (causa não isolada). Busca portável e pacote `0.5.0` estão no PR `#162`. | [Release M2]({{ site.baseurl }}{% link release_m2.md %}) |
| **Limitação — Colunas do CSV `--emit-timeline`** | O contrato CSV possui 5 colunas (`indice`, `inicio_s`, `duracao_s`, `frequencia_hz`, `volume`); eventos `Sample` emitem `frequencia_hz = 0.000000` e `cutoffHz`/`sampleId` existem apenas no `SoundEvent` em memória. | [Relatório Final]({{ site.baseurl }}{% link relatorio_final.md %}) |
| **Limitação — Escopo Sintático Deliberado** | `se`/`entao`/`senao` e `defina`/`retorna` têm tokens no lexer e infraestrutura em C++ (`MAT-06`), mas ficaram fora da gramática `parser.y`; mini-notação `[ ]` (`CMP-05`) e dot-chaining (`CMP-06`) também ficaram fora do escopo. | [Diretrizes de Escopo]({{ site.baseurl }}{% link decisoes_escopo_final.md %}) |

---

## 5. Mapa de Defesa para a Entrevista (Por Frente Técnica e Integrante)

A tabela abaixo consolida os módulos, PRs e tópicos-chave para que cada integrante da equipe defenda com segurança a arquitetura ponta a ponta e suas contribuições principais durante a arguição:

| Integrante | Frentes Principais e Entregas-Chave | Arquivos / Referências para Citar na Entrevista | Pergunta Típica da Banca & Ponto de Defesa |
|---|---|---|---|
| **Arthur Luiz** (`@ArthurLuizUnB`) | Motor DSP, Ambientes/Closures, Volume Master, Filtro BiQuad LPF e Anti-cliques (`MAT-06` PR `#129`, `DSP-04/05/08/09/11/12`, `CMP-06` PR `#132`) | `src/audio/audio_engine.{h,cpp}`, `src/audio/wavetable.h`, `src/audio/biquad_filter.h`, `src/audio/gain_smoother.h`, `src/interpreter/ambiente.{h,cpp}` | **Como os efeitos DSP operam em tempo real sem artefatos?** Filtros RBJ por voz e master bus (`--lpf`), suavização linear (`GainSmoother` 5 ms para `--volume`), wavetable pré-computada e isolamento em trilhas. |
| **Caio Melo Borges** (`@CaioMelo25`) | Semântica, Trilhas Paralelas, Arena da AST, Limites, Master Bus (`LimitadorDePico`) e Qualidade CI (`CMP-13` `#96`, `CMP-07` `#128`, `CMP-08` `#100`, `MAT-10` `#156`, `BUG-AUDIO` `#152`, `SCH-08` `#144`, `CMP-12` `#145`, `DOC-A/B`) | `src/semantic/`, `src/interpreter/arena.{h,cpp}`, `src/audio/limiter.h`, `parser/parser.y`, `docs/gramatica_ebnf.md`, `docs/decisao_modelo_batch.md`, `docs/adr_002_trilhas_paralelas.md` | **Como o compilador garante segurança de memória, escopo e ausência de conflitos?** Gramática LALR(1) `%expect 0` com separador `por`, Arena contígua da AST (`MUS_COM_ASAN=1`), teto de 256 níveis e TSan rodando áudio na CLI. |
| **Cecília Costa** (`@CeciliaCunha`) | Saída-Alvo WAV PCM 16-bit, Integração E2E, *Voice Stealing*/Resampling, Documentação Consolidada e Releases (`COMP-C` `#151`, `INT-06` `#115`, `DSP-06/07`, `DOC-03/04/05/06`, `INT-04`, `INT-11` `#158`, `INT-12`) | `src/codegen/gerador_wav.{h,cpp}`, `tests/saida_wav/verifica.cmake`, `tests/test_integracao.cpp`, `docs/saida_alvo.md`, `docs/arquitetura.md`, `docs/manual_usuario.md`, `docs/relatorio_final.md`, `docs/release_final.md`, `LICENSE` | **Por que WAV PCM 16-bit e como o executável evita divergir do som ao vivo?** Porque preserva microtons, polifonia e *samples* sem reexecutar parsing, compartilhando exatamente `carregarFila`, `segundosParaFrame` e `AudioEngine::render`. |
| **Julia Patricio** (`@juliapat18`) | Samples WAV, Auditoria Real-Time do Callback, Notas Curtas (`#123`), Precisão Matemática Final e Automação Release M2 (`INT-05` `#137`, `SCH-11` `#155`, `CMP-04/05`, `MAT-04/12`, `INT-08` `#146`, `#148`) | `src/audio/sample_bank.{h,cpp}`, `scripts/check_realtime.py`, `scripts/test_check_realtime.py`, `scripts/validar_release.sh`, `docs/auditoria-realtime.md`, `docs/release_m2.md` | **Como provar estaticamente que o callback não aloca nem bloqueia?** `scripts/check_realtime.py` audita 33 funções em 11 arquivos contra `new`, containers, locks, `throw`, I/O e `static` locais sem `constexpr`, rodado no CTest (`sch11_*`). |
| **Marcella Anderle** (`@marcellaanderle`) | Representação Intermediária (`Timeline`), Emissão CSV, Dobra de Constantes, Drift/Neumaier, Açúcar Léxico, Fila SPSC, Profiling e Consumo Ocioso (`COMP-A/B/D`, `CMP-11` `#150`, `MAT-08` `#142`, `SCH-05/07/12`) | `src/core/sound_event.h`, `src/core/playhead.{h,cpp}`, `src/core/timeline_emit.{h,cpp}`, `src/core/ring_buffer.h`, `src/otimizacao/dobra_constantes.{h,cpp}`, `docs/representacao_intermediaria.md`, `docs/otimizacao.md`, `docs/consumo_ocioso.md` | **Qual o papel da IR `Timeline` e como garantir que `COMP-D` não altera o som?** A `Timeline` desacopla tempo musical da síntese com 6 invariantes; a dobra de constantes compartilha `operacoes.{h,cpp}` e produz CSV idêntico byte a byte sob `--sem-otimizacao`. |

---

## 6. Texto Pronto para Publicação em GitHub Releases (`Release Notes`)

```markdown
# Compilador Musical Batch — Entrega Final (Sprint 06)

Compilador completo para a DSL musical `.mus` em português, construído em **C++17**, **GNU Flex**, **GNU Bison** e **CMake**, operando estritamente sob o modelo *batch* de programa finito (ADR 001).

## Principais Capacidades Integradas (Fases 1 a 6)
- **Fase 1 (Léxica — Flex):** tokenização com linha/coluna, notas latinas com acidentes (`#`, `b`) e açúcar sintático para figuras rítmicas (`semibreve` a `semifusa`) e `vez` (`CMP-11`).
- **Fase 2 (Sintática e AST em Arena — Bison):** gramática LALR(1) sem conflitos (`%expect 0`), alocação contígua em `Arena` vinculada a `ast::Arvore` (`MAT-10`) e limite determinístico de 256 níveis de profundidade (`CMP-08`).
- **Fase 3 (Semântica):** inferência estática (`numero`, `nota`, `booleano`, `string`), Tabela de Símbolos (`--simbolos`), escopos em blocos `repita` e trilhas paralelas finitas `{ , }` (`CMP-07`).
- **Fase 4 (Otimização da AST):** dobra de constantes (`COMP-D`), desligável com `--sem-otimizacao` preservando a IR bit a bit.
- **Fase 5 (IR `Timeline`):** emissão determinística em CSV com `--emit-timeline` (`COMP-A`, `COMP-B`), soma compensada de Neumaier no `Playhead` e `segundosParaFrame` (`MAT-08`).
- **Fase 6 (Saída-Alvo WAV e Áudio Real-Time):** geração offline de **WAV PCM 16-bit (44 100 Hz, mono)** com `-o`/`--saida` (`COMP-C`), compartilhando o núcleo DSP (`AudioEngine::render`) com wavetable (`DSP-08`), *samples* WAV (`INT-05`), envelopes adaptativos (`#123`), filtro passa-baixa BiQuad RBJ (`filtro` / `--lpf`, `DSP-11`), *voice stealing* (`DSP-07`), `LimitadorDePico` + *soft-clipper* (`#109`, `DSP-10`), Volume Master suavizado (`--volume`, `DSP-09`) e auditoria estática Real-Time (`SCH-11`).

## Verificação e Documentação
- **Testes:** 39/39 alvos no CTest; 517 casos na suíte padrão `doctest` (530 ativos em `mus_tests --count`, 202.564 asserções sob TSan; 531 casos / 202.567 asserções sob ASan), além de 34 testes do verificador `scripts/check_realtime.py`.
- **Sanitizers & CI:** Ubuntu + macOS (`-DMUS_WERROR=ON`), Cppcheck, ASan/LSan/UBSan e ThreadSanitizer.
- **Licença:** MIT (`LICENSE`).
- **Docs principais:** [Relatório Final](https://interpretador-musical.github.io/Grupo03-COMP01/relatorio_final.html) · [Release Final e Demonstração](https://interpretador-musical.github.io/Grupo03-COMP01/release_final.html) · [Visão de Arquitetura](https://interpretador-musical.github.io/Grupo03-COMP01/arquitetura.html) · [Manual do Usuário](https://interpretador-musical.github.io/Grupo03-COMP01/manual_usuario.html)
```
