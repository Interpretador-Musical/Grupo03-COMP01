---
title: Auditoria Real-Time
layout: default
parent: Documentação
nav_order: 18
---

# SCH-11 — Auditoria estática das restrições Real-Time

## 1. Objetivo

A SCH-11 (#61) implementa uma auditoria estática automatizada para impedir que operações potencialmente bloqueantes, alocações dinâmicas ou chamadas de sistema lentas entrem no caminho crítico de tempo real do callback de áudio (`AudioEngine::dataCallback`).

O verificador vive em `scripts/check_realtime.py` e utiliza apenas a biblioteca padrão do Python 3, sem dependências externas.

---

## 2. Cobertura do caminho de áudio

A auditoria extrai e verifica **33 funções/métodos** distribuídos pelos **11 arquivos** que participam diretamente da execução do callback:

| Arquivo | Funções / métodos auditados |
|---|---|
| `src/audio/audio_engine.cpp` | `AudioEngine::dataCallback`, `AudioEngine::render`, `AudioEngine::renderSine`, `AudioEngine::renderRealTime`, `AudioEngine::allocateVoice`, `AudioEngine::isBetterToSteal` |
| `src/core/timeline_player.cpp` | `TimelinePlayer::render` |
| `src/audio/audio_buffer.cpp` | `AudioBuffer::sampleLinear` |
| `src/audio/sample_bank.cpp` | `SampleBank::get` |
| `src/core/frames.h` | `segundosParaFrame` |
| `src/core/sound_event.h` | `SoundEvent::endTime` |
| `src/core/ring_buffer.h` | `SpscRingBuffer::pop`, `SpscRingBuffer::advance` |
| `src/audio/limiter.h` | `softClip`, `softClipBuffer`, `LimitadorDePico::processar`, `LimitadorDePico::processarBuffer` |
| `src/audio/gain_smoother.h` | `GainSmoother::snapTo`, `GainSmoother::setTarget`, `GainSmoother::next`, `GainSmoother::advance`, `GainSmoother::processBuffer` |
| `src/audio/biquad_filter.h` | `sanitizeFilterCutoff`, `sanitizeFilterQ`, `BiquadFilter::resetState`, `BiquadFilter::disable`, `BiquadFilter::configureLowPass`, `BiquadFilter::processSample`, `BiquadFilter::flushDenormals`, `BiquadFilter::processBuffer` |
| `src/audio/wavetable.h` | `sineTable`, `lookupSine`, `frequencyToPhaseIncrement` |

Se um arquivo listado for removido ou se qualquer assinatura deixar de existir exatamente uma vez, a auditoria falha imediatamente, evitando que refatorações silenciosamente tirem funções da cobertura.

---

## 3. Operações proibidas verificadas

Após mascarar comentários (`//`, `/* ... */`) e literais de string/caractere preservando a numeração de linhas, o analisador reprova:

1. **Alocação e desalocação explícita ou via *smart pointers*:** `malloc`, `calloc`, `realloc`, `free`, `aligned_alloc`, `posix_memalign`, `new`, `delete`, `make_unique`, `make_shared` e `allocate_shared`.
2. **Operações de crescimento/realocação em containers:** chamadas (`.` ou `->`) a `push_back`, `emplace_back`, `push_front`, `emplace_front`, `emplace`, `insert`, `resize`, `reserve`, `shrink_to_fit` e `assign`.
3. **Tipos da biblioteca padrão com alocação dinâmica ou *type erasure*:** `std::string` (e variantes), `std::function`, `std::vector`, `std::deque`, `std::list`, `std::map`, `std::unordered_map`, `std::set`, `std::unordered_set`, `std::stringstream`/`ostringstream`/`istringstream`, `std::fstream`/`ifstream`/`ofstream` e `std::thread`.
4. **Exceções:** `throw` (aloca objeto de exceção via `__cxa_allocate_exception` e percorre tabelas de *unwinding* sob lock global).
5. **Entrada/saída e logging:** `printf`, `fprintf`, `sprintf`, `snprintf`, `vprintf`, `vfprintf`, `puts`, `fputs`, `fwrite`, `fread`, `fopen`, `fclose`, `fflush`, `std::cout`/`cerr`/`clog`/`cin` e as macros `LOG_DEBUG`, `LOG_INFO`, `LOG_WARN` e `LOG_ERROR` (que adquirem mutex de escrita).
6. **Sincronização bloqueante e suspensão de thread:** `mutex` (e variantes), `lock_guard`, `unique_lock`, `scoped_lock`, `shared_lock`, `condition_variable`, `future`, `promise`, `async`, `jthread`, chamadas `lock`/`try_lock`/`unlock`/`wait`/`wait_for`/`wait_until`, além de `sleep`, `usleep`, `nanosleep`, `sleep_for`, `sleep_until`, `system` e `popen`.
7. **Variáveis `static` locais sem `constexpr`:** qualquer declaração `static` local não marcada como `constexpr` (ex.: `static Foo foo;`, `static Buffer v(1024);`, `static float t[]{1, 2};`, lambdas ou chamadas de função), pois o C++17 insere guarda de inicialização protegida por lock (`__cxa_guard_acquire`) na primeira passagem. Declarações `static constexpr` / `constexpr static` são permitidas por terem inicialização constante em tempo de compilação.

Em caso de falha, tanto o cabeçalho `SCH-11: FALHA na auditoria Real-Time` quanto cada violação (`arquivo:linha: assinatura: operação proibida`) são emitidos em `stderr` com código de saída `1`.

---

## 4. Execução local

```bash
# Executar somente a auditoria sobre o repositório
python3 -B scripts/check_realtime.py

# Executar a suíte de testes unitários do verificador (34 casos)
python3 -B -m unittest discover -s scripts -p 'test_check_realtime.py' -v

# Executar ambos via CTest
ctest --test-dir build -R '^sch11_' --output-on-failure
```

---

## 5. Integração com CMake e CI

Em `tests/CMakeLists.txt`, o interpretador Python 3 é buscado de forma **opcional** (`find_package(Python3 COMPONENTS Interpreter)` sem `REQUIRED`):

- Quando `Python3_FOUND` é verdadeiro (como nos runners Ubuntu e macOS da CI), os testes `sch11_auditoria_realtime` e `sch11_testes_verificador` entram automaticamente no CTest.
- Numa máquina sem Python 3 instalado, a configuração e a compilação C++ seguem normalmente, apenas omitindo os dois alvos de script.

---

## 6. Inicialização da wavetable (`src/audio/wavetable.h`)

Na versão anterior à SCH-11, `sineTable()` continha uma variável `static const` local inicializada por lambda na primeira chamada — o que introduzia uma guarda `__cxa_guard_acquire` dentro do caminho de síntese caso a primeira nota fosse renderizada diretamente na thread de áudio.

A tabela passou a ser definida como variável `inline const kSineTable` em escopo de *namespace*. Em C++17, sua construção ocorre na fase de inicialização estática do processo, **antes da entrada em `main()`**, e `sineTable()` apenas devolve uma referência constante à tabela já pronta, sem guarda local nem necessidade de chamada manual no construtor de `AudioEngine`.

---

## 7. Limitações conhecidas

Por operar sobre texto com expressões regulares e balanceamento de chaves (sem front-end completo de C++):

- **Escopo explícito (`TARGETS`):** funções auxiliares novas chamadas pelo callback precisam ser adicionadas a `TARGETS` em `scripts/check_realtime.py` (o script detecta automaticamente remoções ou renomeações das funções já listadas, mas não descobre sozinho um novo arquivo auxiliar).
- **Sobrecargas e tipos customizados:** um tipo próprio ou *alias* (`using MeuVetor = std::vector<float>;`) ou um operador sobrecarregado que aloque memória fora dos nomes monitorados não será flagrado pelo texto da função chamadora.
- **Macros complexas e ponteiros de função:** chamadas indiretas ou macros que escondam chaves desbalanceadas podem escapar ou exigir ajuste no extrator.
- **Complementaridade:** esta verificação estática atua em conjunto com as garantias em tempo de compilação (`static_assert` de `is_always_lock_free` e `is_trivially_copyable` em `SpscRingBuffer` e `SoundEvent`) e com o job **ThreadSanitizer** da CI (`SCH-08`, #46).

---

## 8. Resultados de validação

- **33 funções críticas** verificadas em 11 arquivos (`0` violações);
- **34 testes unitários** em `scripts/test_check_realtime.py` aprovados;
- **39 testes no CTest** aprovados em compilação com `-DMUS_WERROR=ON` e `cppcheck` limpo.

---

## 9. Referências internas

- Issue: `SCH-11` (#61)
- Verificador: `scripts/check_realtime.py`
- Testes do verificador: `scripts/test_check_realtime.py`
- Integração CTest: `tests/CMakeLists.txt`
- Wavetable: `src/audio/wavetable.h`
