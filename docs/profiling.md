---
title: Profiling de CPU e Cache
layout: default
parent: Documentação
nav_order: 13
---

# Profiling de CPU e cache (Callgrind)

Este documento registra onde o compilador e o callback de áudio gastam CPU, e se o acesso à memória causa cache misses (SCH-07, #41). A regra foi medir antes de otimizar: nenhuma estrutura foi refatorada sem um número que mostrasse o gargalo.

---

## 1. O que foi medido, e por quê

No modelo batch ([ADR 001]({{ site.baseurl }}{% link decisao_modelo_batch.md %})), um programa `.mus` passa por duas partes com custo de CPU:

1. **A compilação** (lexer → parser → semântica → interpretador → emissão do IR). Roda uma vez, antes do áudio, e não tem prazo de tempo real.
2. **O callback de áudio** (`AudioEngine::render`). É a única parte com restrição de tempo real: cada bloco de 256 frames precisa ficar pronto em menos de 5,8 ms, senão o som falha.

A thread de tempo (`Engine`) não foi medida: ela só é usada pelo `--loop` e fica fora do caminho de um programa `.mus`. O ring buffer também não aparece como alvo próprio. A fila é carregada inteira antes do `start()` (SCH-05, #29), então durante a reprodução nenhuma outra thread disputa os índices com o callback.

As cargas estão em `tests/test_perfil.cpp`, em dois casos pulados por padrão (`doctest::skip`) para não deixar o CTest lento:

| Carga | O que faz |
|---|---|
| `perfil: compilar e emitir um programa de 20 mil linhas` | 20 000 linhas `toca do4 + k por t` passando por todas as fases, até o CSV do `--emit-timeline`. São linhas de verdade, porque um `repita 20000` não exercitaria o lexer nem o parser |
| `perfil: renderizar 10 s de áudio com todas as vozes ocupadas` | O pior caso do callback: um acorde de 32 notas (`kMaxVoices`) a cada meio segundo, renderizado em blocos de 256 frames. Todas as vozes ficam sempre ativas e cada acorde novo passa pelo voice stealing (DSP-07) |

---

## 2. Ambiente

| Item | Valor |
|---|---|
| CPU | Intel Core i7-4510U @ 2,00 GHz (2 núcleos, 4 threads, notebook de 2014) |
| Cache | L1d 32 KiB por núcleo, L2 256 KiB por núcleo, L3 4 MiB |
| Compilador | GCC 13.3, `-DCMAKE_BUILD_TYPE=RelWithDebInfo` (`-O2 -g`) |
| Valgrind | 3.22.0 |
| Data | 03/11/2026 |

O build é otimizado de propósito: perfilar sem `-O2` mostra gargalos que não existem no binário real.

---

## 3. Resultados

### 3.1 Tempo real (fora do Valgrind)

Três execuções seguidas, sem Valgrind:

| Carga | Tempo | Leitura |
|---|---|---|
| Compilar e emitir 20 000 linhas | 48–52 ms | — |
| Renderizar 10 s com 32 vozes | 307–310 ms | **3,1% de um núcleo** |

O callback, no pior caso de polifonia, usa 3,1% de um núcleo de um processador de notebook de 2014. Isso fica abaixo dos 5% que a issue pedia como teto, com folga de cerca de 30 vezes em relação ao prazo do bloco.

### 3.2 Compilação: custo por fase (Callgrind)

Total de 334 milhões de instruções (`Ir`). Os valores são inclusivos, isto é, cada fase inclui as funções que ela chama:

| Fase | Instruções | Parcela |
|---|---|---|
| Emissão do CSV (`emitirTimeline`) | 165,6 M | 49,6% |
| Lexer + parser (`parseToAst`, `yyparse`) | 102,6 M | 30,7% |
| ↳ só o lexer (`yylex`) | 74,0 M | 22,2% |
| Liberação da AST (`ast::liberar`) | 21,7 M | 6,5% |
| Interpretador (`Interpretador::executar`) | 20,7 M | 6,2% |
| Análise semântica (`AnalisadorSemantico::analisar`) | 7,2 M | 2,1% |

Metade da compilação é formatar números no CSV: o `snprintf` com `%f` sozinho custa 47% do total. As fases do compilador propriamente dito (lexer, parser, semântica e interpretador) somam cerca de 40%.

### 3.3 Callback de áudio (Callgrind)

Total de 1,94 bilhão de instruções para 441 000 frames × 32 vozes, cerca de **137 instruções por voz por amostra**. Valores exclusivos (só o código da própria função):

| Função | Instruções | Parcela |
|---|---|---|
| `std::sin` (libm, `__sin_fma`) | 1 260 M | 65,0% |
| `AudioEngine::renderRealTime` (envelope, mixagem, disparo de eventos) | 600,6 M | 31,0% |
| Limitador (`softClip`: `tanhf` + `expm1f`) | 36 M | 1,8% |

O `std::sin` em `double`, chamado uma vez por voz a cada amostra, é o ponto quente do callback.

### 3.4 Cache misses (Cachegrind)

| Carga | Leituras + escritas de dados | Misses na L1d | Taxa L1d | Taxa no último nível |
|---|---|---|---|---|
| Renderizar 10 s com 32 vozes | 754 M | 33 513 | **0,0%** | 0,0% |
| Compilar e emitir 20 000 linhas | 139 M | 1,29 M | 0,9% | 0,4% |

O callback praticamente não tem cache miss. O estado inteiro do sintetizador (32 `Voice` num `std::array`, cerca de 2,5 KiB) cabe folgado na L1 de 32 KiB e é percorrido em ordem a cada amostra. Na compilação, os misses de leitura vêm sobretudo de percorrer a AST (semântica, interpretador e `liberar` visitando os nós), porque cada nó é alocado separadamente com `new` e fica espalhado no heap. Ainda assim, 0,9% é uma taxa baixa.

---

## 4. Decisão: nenhuma otimização agora

A issue original pedia para refatorar estruturas críticas, como o ring buffer, para melhorar a localidade de cache. As medições não mostram motivo para isso:

- **Cache:** o callback não tem misses que valha a pena reduzir, e o ring buffer nem aparece no perfil. O caso clássico de *false sharing* entre `writeIndex_` e `readIndex_` não acontece no modelo batch, porque o produtor terminou antes de o consumidor começar.
- **CPU:** o pior caso usa 3,1% de um núcleo, abaixo do teto de 5%.

Refatorar sem gargalo arriscaria introduzir defeitos no áudio (BUG-AUDIO, #109, ainda aberta) sem ganho mensurável.

**Se um dia for preciso**, os dados apontam onde mexer, em ordem de ganho esperado:

1. **Oscilador** (65% do callback): trocar o `std::sin` por amostra por um oscilador recursivo ou uma tabela de onda. Isso muda levemente as amostras geradas, então exige rever os testes de áudio e ouvir o resultado.
2. **Incremento de fase:** `renderRealTime` recalcula `(2π · f) / 44100`, com uma divisão, para cada voz a cada amostra. Esse valor poderia ser guardado na `Voice` quando a nota dispara.
3. **Emissão do CSV** (metade da compilação): formatar os `double` sem passar pelo `snprintf`. Não tem prazo de tempo real, então só vale se o `--emit-timeline` ficar lento em programas reais.

---

## 5. Como reproduzir

```bash
# build otimizado com símbolos de debug, numa pasta separada
cmake -S . -B build-prof -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build-prof -j

# tempo real (fora do Valgrind)
./build-prof/tests/mus_tests --no-skip --test-case="perfil:*"

# CPU por função
valgrind --tool=callgrind --callgrind-out-file=compilacao.out \
  ./build-prof/tests/mus_tests --no-skip --test-case="perfil: compilar*"
valgrind --tool=callgrind --callgrind-out-file=render.out \
  ./build-prof/tests/mus_tests --no-skip --test-case="perfil: renderizar*"
callgrind_annotate --inclusive=yes compilacao.out | head -60
callgrind_annotate render.out | head -30
kcachegrind render.out   # visual, com árvore de chamadas

# cache misses
valgrind --tool=cachegrind --cache-sim=yes --cachegrind-out-file=cache.out \
  ./build-prof/tests/mus_tests --no-skip --test-case="perfil: renderizar*"
cg_annotate cache.out | head -30
```

Os números absolutos mudam de máquina para máquina. As proporções entre as funções e as taxas de miss devem se manter.
