---
title: Análise estática e avisos
layout: default
parent: Documentação
nav_order: 17
---

# Análise estática e avisos do compilador

Este documento registra a revisão da CMP-12 (#64). Ela cobre que avisos do compilador o projeto exige, o que o Cppcheck encontrou e o que fica de fora, com o motivo de cada decisão. O roadmap pede uma revisão "proporcional ao risco", e esse foi o critério para cada flag e cada supressão.

---

## 1. Antes da CMP-12

Só o `mus_tests` compilava com `-Wall -Wextra` (`tests/CMakeLists.txt`). A biblioteca `mus_core`, o frontend e o executável `compilador` compilavam sem flag de aviso nenhuma. O [Release M1]({{ site.baseurl }}{% link release_m1.md %}) registra uma compilação manual com `-Wall -Wextra -Werror` sem aviso, mas nada no CMake ou no CI mantinha isso. O Cppcheck nunca tinha rodado.

## 2. O que o projeto exige

Todo alvo nosso liga a biblioteca INTERFACE `mus_avisos` (`CMakeLists.txt`). Isso vale para `mus_core`, `mus_lexer`, `compilador`, `mus_tests` e `mus_estresse`. O conjunto é:

| Flag | O que pega |
|---|---|
| `-Wall -Wextra` | a base: variável não usada, comparação com sinal misturado, parâmetro ignorado etc. |
| `-Wpedantic` | extensões fora do C++17 padrão |
| `-Wshadow` | nome local escondendo outro de escopo maior |
| `-Wnon-virtual-dtor` | classe com método virtual e destrutor não virtual |
| `-Woverloaded-virtual` | sobrecarga que esconde um virtual da base |
| `-Wold-style-cast` | cast no estilo C, que esconde qual conversão acontece |
| `-Wcast-align` | cast que aumenta o alinhamento exigido |
| `-Wnull-dereference` | caminho que desreferencia nulo (o GCC só detecta com otimização) |
| `-Wformat=2` | `printf` com formato não literal ou argumento incompatível |
| `-Wimplicit-fallthrough` | `case` que cai no seguinte sem estar marcado |

No MSVC o equivalente é `/W4`.

A opção `MUS_WERROR` transforma aviso em erro (`-Werror` ou `/WX`). Ela fica **desligada por padrão**, para que um compilador mais novo, com avisos novos, não trave a build de quem só quer compilar. O **CI a liga** no job de build, no GCC do Ubuntu e no clang do macOS.

**Ficam de fora:**

- **O código gerado pelo Flex e pelo Bison** (`lex.yy.cpp`, `parser.tab.cpp`), compilado com `-w`. O código não é nosso, e o Bison 2.3 do macOS e o 3.8 do Ubuntu geram código diferente. O único aviso desse grupo na medição era um `-Wsign-compare` dentro do `lex.yy.cpp`. O `lexer.l` e o `parser.y` continuam revisados; só a saída do gerador fica em silêncio.
- **Os cabeçalhos vendorizados em `include/`** (miniaudio e doctest), que entram como `SYSTEM`. Só a miniaudio gerava mais de 6 mil ocorrências de `-Wold-style-cast` no conjunto acima.

## 3. Medição

Cada unidade de tradução de `src/` e `tests/` (64 ao todo) foi compilada com `-fsyntax-only` a partir do `compile_commands.json`, com `include/` como cabeçalho de sistema. A medição foi feita em 21/11/2026, sobre a `main` em `eaf014f`.

| Conjunto | Apple clang 21 (SDK macOS 26) | GCC 15.2 (Homebrew) |
|---|---:|---:|
| Exigido (seção 2) | 1 | 1 |
| `-Wconversion -Wsign-conversion` | 17 | 19 |

O GCC no macOS não compila o `audio_engine.cpp`, porque os cabeçalhos do CoreAudio no SDK da Apple não são para GCC. Para esse arquivo, a referência de GCC é o CI (GCC 13, Ubuntu).

**O único aviso do conjunto exigido** era um `-Wshadow`. O `kEpsilon` local de `AudioEngine::isBetterToSteal` (1e-4, tolerância de amplitude) escondia o `mus::kEpsilon` de `core/pitch.h` (1e-9, tolerância de tempo e frequência). Eram duas tolerâncias diferentes com o mesmo nome, e a local passou a se chamar `kEmpateDeAmplitude`.

**`-Wconversion` e `-Wsign-conversion` foram medidas, mas não são exigidas.** Nenhuma das ocorrências é defeito:

- 15 são o mesmo padrão: índice `int` em `voices_[i]`, 14 no `audio_engine.cpp` e 1 num teste. O índice é sempre limitado a `[0, kMaxVoices)` pelo clamp do `setMaxVoices`, então nunca chega negativo ao `size_t`.
- 1 é um índice `int`, não negativo, de `std::vector` em `tests/test_audio_envelope.cpp`.
- As restantes (1 no clang, 3 no GCC) são testes convertendo uma contagem (`size_t` ou `int`) para `double` ou `float` numa conta, com valores muito menores que o limite de precisão.

Exigir essas flags pediria um `static_cast` em cada um desses pontos, sem pegar defeito nenhum, e o `audio_engine.cpp` também está aberto em outros PRs. A contagem fica registrada aqui para ser refeita se o código de índices mudar.

## 4. Cppcheck

O job `cppcheck` do CI roda:

```bash
cppcheck --enable=warning,style,performance,portability \
  --std=c++17 --language=c++ --inline-suppr --error-exitcode=1 \
  --suppress=missingIncludeSystem \
  --suppress=useStlAlgorithm --suppress=shadowFunction \
  -q -I src src
```

A análise cobre `src/`, o código de produção. `include/` não entra no `-I`, para o Cppcheck não analisar as 95 mil linhas da miniaudio. A versão de referência é a do `apt` do Ubuntu, a do CI. A medição local foi feita com a 2.22.0, que achou 9 pontos, todos da categoria *style*, nenhum `warning`:

| Regra | Ocorrências | Decisão |
|---|---:|---|
| `useStlAlgorithm` | 6 | suprimida. Ela só sugere trocar um laço por `std::accumulate`, `find_if`, `count_if` ou `any_of`. É uma preferência de estilo, não um defeito, e três dos laços estão no callback de áudio, onde um laço simples sobre o pool de vozes é a forma mais legível. |
| `shadowFunction` | 3 | suprimida. Dois casos são o padrão `setBpm(double bpm)`, com o parâmetro de mesmo nome do getter `bpm()`; o terceiro é um local `simbolo` com o mesmo nome do método `simbolo()`. O sombreamento que pode causar defeito, o de variável, já é cobrado pelo `-Wshadow` do compilador. |

Com as duas supressões, o Cppcheck sai sem achados em `src/`.

### Código morto

Uma passagem única com `--enable=unusedFunction` sobre `src/` e `tests/` (fora do CI, porque a regra exige o programa inteiro e é ruidosa com bibliotecas) apontou:

| Função | Situação |
|---|---|
| `avancarPosicao`, `registrarNumero` e `registrarNota` (`frontend.cpp`) | falso positivo: são chamadas das ações do `lexer.l`, e o Cppcheck não analisa o código gerado |
| `AudioEngine::setToneFrequency` | sem chamador; o `--test-tone` usa a frequência padrão de 440 Hz |
| `AudioEngine::activeVoiceCount` | sem chamador na `main`; o PR #144 (SCH-08) passa a usá-la nos testes |

Mais dois pontos ficam registrados para o grupo decidir. **Nada foi removido.**

- **`AudioEngine::loadTimeline` e o `TimelinePlayer`.** O `main.cpp` não os usa, porque a reprodução vai pela fila SPSC desde a SCH-05 (#29). Eles continuam testados, e o PR #142 (MAT-08) está mexendo no `TimelinePlayer`.
- **`core/arc.h`, `core/hap.h` e `core/pattern.h`.** São desenho do modelo de padrões descartado em 16/09 ([ADR 001]({{ site.baseurl }}{% link decisao_modelo_batch.md %})). Ficaram de propósito, porque a subdivisão de `Arc::cycles()` serve às tercinas e síncopes.

## 5. Como reproduzir

Os comandos para compilar com `-Werror` e rodar o Cppcheck localmente estão no [Guia de ambiente]({{ site.baseurl }}{% link guia_ambiente.md %}), seção 5.
