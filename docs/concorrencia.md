---
title: Concorrência e ThreadSanitizer
layout: default
parent: Documentação
nav_order: 16
---

# Concorrência e ThreadSanitizer

Este documento registra quais threads o compilador sobe, que estado elas dividem e como esse estado é protegido. Registra também a varredura com o ThreadSanitizer (TSan) da SCH-08 (#46): o que rodou, o que foi encontrado e o que mudou.

Uma corrida de dados não falha toda vez. Por isso um teste comum não basta, e a verificação é feita com o TSan. Ele marca qualquer par de acessos à mesma memória, por threads diferentes e sem ordem entre si, mesmo que naquela execução nada tenha dado errado.

---

## 1. As threads

| Thread | Quando existe | O que faz |
|---|---|---|
| Principal | sempre | lê o arquivo, compila até a Timeline, carrega a fila e espera a música terminar (`waitUntilFinished`) |
| Áudio | quando há reprodução (arquivo `.mus`, `--test-tone`, `--loop`) | é criada pelo miniaudio; roda o `dataCallback`, que consome a fila e sintetiza as vozes |
| Tempo | só no `--loop` | é o `Engine` (SCH-01, #4); avança o playhead a cada volta |
| Tratador de sinal | ao chegar SIGINT ou SIGTERM | roda **em qualquer thread** que não bloqueie o sinal, e só marca a flag de parada |

No caminho principal (`compilador musica.mus`), a fila recebe a Timeline inteira **antes** de o áudio começar, e nada mais é empurrado depois (SCH-05, #29). Enquanto a música toca, a thread principal só dorme e lê a bandeira de underrun.

## 2. Estado compartilhado

| Estado | Escreve | Lê | Mecanismo |
|---|---|---|---|
| Fila `SpscRingBuffer<SoundEvent>` | principal (antes do `start`) | áudio | índices atômicos com acquire/release (SCH-03, #17) |
| `frequency_`, `gain_` (`--test-tone`) | principal | áudio | `std::atomic` |
| `maxVoices_` | principal (só parado) | áudio | `std::atomic<int>` |
| `sufferedUnderrun_` | áudio | principal | `std::atomic<bool>`, lido e zerado com `exchange` |
| `framesRenderizados_` | áudio | principal | `std::atomic<uint64_t>`, publicado a cada callback |
| `voices_`, `currentFrame_`, evento pendente | só a thread de áudio | — | a thread principal só encosta neles com o dispositivo parado |
| `ringBuffer_`, `sampleBank_`, `expectedDuration_`, programa carregado | principal, **antes** do `start()` | áudio | publicados pelo próprio `ma_device_start`, que sincroniza com a thread de áudio |
| Posição e estatísticas do `Engine` | tempo | principal | `mutex_` e `condition_variable` |
| `running_`, `tickCount_` do `Engine` | principal e tempo | ambas | `std::atomic` |
| Nível do logger | principal | todas | `std::atomic<LogLevel>`; a escrita da linha é protegida por `g_writeMutex` |
| Flag de parada (`shutdownRequested`) | tratador de sinal | principal | `std::atomic<bool>` sem lock, desde a SCH-08 |

A regra que vale para o `AudioEngine` é a mesma em todos os setters. `loadTimeline`, `setSampleBank`, `setRingBuffer`, `setExpectedDuration` e `setMaxVoices` só podem ser chamados com o dispositivo parado. `setMaxVoices` devolve `false` se o áudio estiver tocando, no mesmo contrato do `Engine::setBpm`.

## 3. Como rodar

Localmente:

```bash
cmake -S . -B build-tsan -DCMAKE_BUILD_TYPE=Debug -DENABLE_TSAN=ON
cmake --build build-tsan --parallel
TSAN_OPTIONS=halt_on_error=1 ./build-tsan/tests/mus_tests
./build-tsan/compilador -v exemplos/polirritmia.mus
```

No CI, o job **ThreadSanitizer** (`.github/workflows/ci.yml`) roda nesta ordem:

1. compila com a mesma opção `-DENABLE_TSAN=ON`;
2. roda o `mus_tests` inteiro com `halt_on_error=1`;
3. toca, ainda instrumentados, os três exemplos, a senoide do `--test-tone` e o `--loop` interrompido por SIGINT.

O runner do GitHub não tem placa de som. Nessa situação, o `ma_device_init` do miniaudio tenta cada backend e cai no **null**, que roda o callback numa thread própria, em tempo real. A reprodução é a mesma de uma máquina com som, só que muda.

O passo reprova se a saída tiver qualquer relato do TSan. Reprova também se tiver "seguindo sem áudio", porque nesse caso a thread de áudio não chegou a existir e a execução não provaria nada. O log de debug imprime o backend usado.

A suíte `concorrencia` (`tests/test_concorrencia.cpp`) tem entrada própria no ctest (`mus_tests_concorrencia`). Os testes dela usam `AudioEngine::Saida::Nula`, que abre o backend null direto. Assim eles exercitam a thread de áudio de verdade em qualquer máquina, sem tocar som.

## 4. O que a varredura encontrou

Antes da SCH-08, todos os caminhos que a CLI executa já saíam limpos sob o TSan:

- a suíte inteira;
- os três exemplos tocando pelo CoreAudio;
- o `--test-tone`;
- o `--loop` interrompido por SIGINT (três execuções).

As duas corridas encontradas ficam em caminhos que nenhuma execução da CLI percorre. Por isso cada uma ganhou um teste que a provoca.

### 4.1 `setMaxVoices` com o áudio tocando

A função limpava as vozes acima do novo limite a partir da thread de quem chamava, enquanto o callback lia e escrevia essas mesmas vozes. Além da corrida, a voz era zerada no meio da onda, o que dá um clique.

Nada em `src/` chamava a função com o áudio tocando, então o defeito estava latente. O teste "setMaxVoices com o áudio tocando é recusado e não encosta nas vozes" chama a função com quatro vozes soando. Na versão anterior à correção, ele gerava **5 relatos**. Este é o primeiro, com os caminhos encurtados:

```text
WARNING: ThreadSanitizer: data race
  Write of size 4 by main thread:
    #0 mus::AudioEngine::setMaxVoices(int) audio_engine.cpp:124
    #1 ... test_concorrencia.cpp:77
  Previous write of size 4 by thread T2:
    #0 mus::AudioEngine::renderRealTime(float*, unsigned int) audio_engine.cpp
    #1 mus::AudioEngine::render(float*, unsigned int) audio_engine.cpp:180
    #2 mus::AudioEngine::dataCallback(...) audio_engine.cpp:174
    ...
    #7 ma_worker_thread(void*) miniaudio.h:42763
  Thread T2 (running) created by main thread at:
    ...
    #2 ma_device_init miniaudio.h:43931
    #3 mus::AudioEngine::init() audio_engine.cpp:53
SUMMARY: ThreadSanitizer: data race audio_engine.cpp:124 in mus::AudioEngine::setMaxVoices(int)
```

**Correção.** Com o dispositivo rodando, `setMaxVoices` devolve `false` e não muda nada. Parado, funciona como antes. Depois da correção, o mesmo teste sai sem relato.

### 4.2 Flag de parada escrita por um tratador de sinal

O `--loop` termina com Ctrl+C. O tratador marcava um `volatile std::sig_atomic_t`, e a thread principal o lia em `runUntilStopped()`. O sinal, porém, pode cair na thread de tempo ou na de áudio. `volatile sig_atomic_t` só tem comportamento definido quando o tratador e o leitor estão na mesma thread. Entre threads diferentes, é corrida de dados.

**Correção.** A flag virou `std::atomic<bool>`, com `static_assert` de que é sem lock. O C++17 (\[support.signal\]) permite atômico sem lock dentro de um tratador de sinal. Também foi criada a `clearShutdownRequest()`, que deixa o laço rodar mais de uma vez no mesmo processo.

**O TSan não acusa este caso.** Ele sincroniza a entrada no tratador de sinal. O silêncio se repetiu com o sinal entregue de forma síncrona (`raise` em outra thread) e de forma assíncrona (`kill` para o processo, com a thread principal bloqueando o sinal). A correção se apoia na norma, não num relato. O teste "o pedido de parada feito por sinal em outra thread chega à principal" confere o comportamento e cobre os tratadores do `engine.cpp`, que ficaram sem cobertura desde a Sprint 1.

### 4.3 Reprodução completa numa thread real

O terceiro teste compila um programa com trilhas paralelas e o toca inteiro pelo backend null. Depois do `stop()`, ele confere que a fila esvaziou e que todas as vozes terminaram. A espera é pelo progresso do callback (`framesRenderizados()`) e não pelo relógio. Sob o TSan a thread de áudio fica várias vezes mais lenta, e um prazo medido em segundos de música cortaria o fim.

## 5. Limites

- O TSan só enxerga o caminho que foi executado. Uma corrida num trecho que nenhum teste ou exemplo percorre continua invisível; foi exatamente o caso do `setMaxVoices`.
- No macOS, o CoreAudio não é instrumentado. Um relato só com frames dele seria suspeito de falso positivo. A referência é o Linux do CI, com o backend null, que é código do miniaudio compilado junto e instrumentado.
- O backend null não reproduz a temporização de uma placa real. Ele serve para verificar sincronização, não latência nem underrun de hardware.
