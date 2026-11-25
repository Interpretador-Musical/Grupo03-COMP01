---
title: Consumo Ocioso
layout: default
parent: Documentação
nav_order: 20
---

# Consumo ocioso de CPU (SCH-12, #66)

Este documento registra quanto de CPU o compilador gasta quando não está produzindo som, que laços de espera ele tem e que primitiva cada um usa. Registra também as duas mudanças feitas no callback de áudio e como foi conferido que elas não alteram o som.

O texto original da issue fala de "a linguagem não estar produzindo som ativo", o que pressupõe execução contínua, ao vivo. Desde a [decisão batch]({{ site.baseurl }}{% link decisao_modelo_batch.md %}) (seção 5.4), a leitura é outra: o programa é finito, e o silêncio que existe é o **intervalo entre eventos**, ou seja, as pausas e as lacunas entre notas. É esse intervalo que foi medido e otimizado.

---

## 1. Resumo

| Pergunta | Resposta |
|---|---|
| Há espera ativa (laço girando sem bloquear)? | **Não.** Os quatro laços de espera bloqueiam no sistema operacional (seção 2). Nenhum usa `yield`, que seria espera ativa disfarçada. |
| Quanto custa o callback de áudio no silêncio? | **Antes:** o mesmo que uma nota tocando, cerca de 1,9 ms de CPU por segundo de áudio. **Depois:** 0,04 ms, 42 vezes menos (seção 3). |
| O som mudou? | Não, nos casos comuns: saída idêntica bit a bit em 7 programas e 4 tamanhos de bloco. Mudou só onde havia defeito: a pausa roubava uma voz audível com as vozes cheias, e uma nota de volume NaN contaminava a mixagem (seção 4). |
| E o processo inteiro? | Tocando um programa, o processo gasta cerca de 2% de um núcleo, quase tudo no cliente de áudio do sistema (miniaudio e PulseAudio), e não no código do projeto (seção 5). |

## 2. Os laços de espera

| Laço | Onde | Primitiva | Despertares por segundo | Por que esse intervalo |
|---|---|---|---|---|
| Espera da música (`waitUntilFinished`) | thread principal, toda reprodução | `sleep_for(50 ms)` | 20 | A cada volta lê a bandeira de underrun e a registra no log. O fim é medido contra `steady_clock`, então o intervalo não acumula erro. |
| Laço do `--loop` (`runUntilStopped`) | thread principal, só `--loop` | `sleep_for(20 ms)` | 50 | A flag de SIGINT não pode acordar uma `condition_variable` (chamar `notify` num tratador de sinal é comportamento indefinido). 20 ms faz o Ctrl+C parecer imediato. |
| Thread de tempo (`Engine::timeLoop`) | só `--loop` | `condition_variable::wait_for(1 ms)` com predicado | ~900 | A resolução de 1 ms é a da SCH-01 (#4). A espera com predicado faz o `stop()` acordar a thread na hora. |
| Callback de áudio | thread da miniaudio | o próprio backend, que acorda a cada período | ~170 (256 frames a 44,1 kHz) | É o dispositivo que pede os blocos. |

**Por que não `yield`.** A issue sugere `std::this_thread::yield()`. Um laço com `yield` não dorme: devolve o processador só se outra thread estiver pronta, e volta em seguida. Sem concorrência, ele ocupa um núcleo inteiro. As esperas acima bloqueiam de verdade, e o processo fica parado até o prazo vencer.

**Medido em Release, com o backend null do miniaudio, sem placa de som:**

| Cenário | CPU do processo |
|---|---|
| `Engine` com tick de 1 ms, 2 s | 2,8% |
| `waitUntilFinished` durante 2 s de pausa | 1,1% |

Os testes da suíte `ocioso` (`tests/test_ocioso.cpp`) repetem essas duas situações e exigem menos de 50% de CPU. Uma espera ativa daria perto de 100%. O limite tem folga para os jobs de sanitizer do CI, em que cada volta é várias vezes mais lenta.

**Ficou como está.** O `Engine` acorda cerca de 900 vezes por segundo para não fazer nada no `--loop`, porque nenhum consumidor usa o tick. Aumentar o intervalo mudaria a resolução definida na SCH-01. Como o `--loop` está fora do caminho de um programa `.mus`, a decisão fica registrada aqui e não foi tomada nesta issue.

## 3. O callback no silêncio

### 3.1 O que acontecia

O `renderRealTime` faz, para cada amostra:

1. **Bloco A:** confere se o próximo evento já venceu.
2. **Bloco B:** percorre as 32 vozes (`kMaxVoices`), aplica envelope e oscilador às ativas e soma.

No silêncio, o Bloco A respondia "ainda não é a hora" e o Bloco B visitava 32 vozes paradas para somar zero, 44 100 vezes por segundo. O custo era o mesmo de uma nota tocando.

A pausa piorava o quadro. O interpretador a emite como um evento Synth de 0 Hz e volume 0, e o motor alocava uma voz para ela. Essa voz passava pelo envelope e pelo oscilador a cada amostra, só para multiplicar por zero.

### 3.2 O que mudou

- **Evento silencioso não ocupa voz.** Na hora de disparar, um evento com volume 0 (ou NaN), ou um Synth de 0 Hz, é descartado. O motor só guarda o frame em que ele termina (`fimDoSilencio_`). Até ali, a fila vazia não conta como underrun, que era o papel que a voz da pausa cumpria (o teste "pausa curta no fim não gera falso positivo", da DSP-123, continua passando).
- **Atalho do silêncio.** O Bloco B agora registra se alguma voz continua soando (`vozesAtivas_`). Se nenhuma soa e o próximo evento ainda não venceu, o trecho até ele, ou até o fim do bloco, sai de uma vez como zeros, sem visitar as vozes. A conta é a mesma que o laço faria frame a frame: o `currentFrame_` avança o mesmo número de frames e nenhuma voz muda de estado.

O atalho só vale com um evento pendente. Com a fila vazia, ele não pula: um evento empurrado no meio do bloco precisa ser visto no frame seguinte, como antes.

### 3.3 Medição

Custo do `render()` por segundo de áudio, em microssegundos (mediana de 5 execuções de 20 s, `-O3`, 256 frames por bloco):

| Cenário | Antes | Depois |
|---|--:|--:|
| Silêncio até um evento futuro | 1849 | 44 |
| Pausa seguida de nota | 2019 | 43 |
| Staccato: 0,25 s de nota e 0,75 s de pausa | 2084 | 668 |
| 1 nota | ~1800 | ~1800 |
| 8 notas | ~2700 | ~2700 |

Com som, a diferença fica dentro do ruído da medição (±5% entre execuções). O que sobra no silêncio (44 µs/s) é o master bus (`softClipBuffer`) passando sobre os zeros.

## 4. O som não mudou

### 4.1 Equivalência

O mesmo programa foi renderizado pelo motor da `main` e pelo motor novo, e as amostras `float` foram comparadas byte a byte. Programas: os três de `exemplos/` e quatro casos com silêncio (pausas entre notas, volume 0 no meio de trilhas, samples com pausas e notas de 2 a 10 ms separadas por pausas). Blocos de 1, 7, 256 e 4096 frames, porque o atalho pula até o fim do bloco e um erro de fronteira mudaria com o tamanho dele.

**Resultado:** 28 de 28 combinações idênticas, sem nenhum underrun em nenhuma das duas versões.

Os testes "o silêncio pulado não depende do tamanho do bloco" e "depois de um silêncio longo, a nota começa no frame agendado" fixam essa propriedade. Para conferir que eles pegam um defeito, o pulo foi alterado de propósito para um frame a mais. Os dois falharam, e mais três testes da suíte junto.

### 4.2 Onde mudou, e por quê

| Caso | Antes | Depois |
|---|---|---|
| 32 notas soando e uma pausa numa 33ª trilha | A pausa roubava a voz mais fraca: uma nota do acorde sumia, com o fade-out de 2 ms da DSP-07. Duas pausas em sequência roubavam duas notas. | A pausa não ocupa voz, e o acorde sai idêntico ao acorde sem a trilha de pausas. |
| Nota com volume NaN | A voz somava NaN na mixagem, e todas as vozes do trecho saíam mudas (o `softClip` troca NaN por silêncio). | O evento é descartado, e as outras vozes soam normalmente. |

A Timeline não mudou: a pausa continua sendo um evento do IR ([Representação intermediária]({{ site.baseurl }}{% link representacao_intermediaria.md %})) e continua aparecendo no `--emit-timeline`. A mudança é só em como o motor de áudio a consome.

## 5. O processo inteiro

CPU do processo `compilador` tocando de verdade pelo PulseAudio (Linux, build Release), numa janela de 7 s depois de 2,5 s de aquecimento. É a média de três execuções, alternando o binário da `main` e o desta issue. Os programas usam `volume 0` para não tocar som alto durante a medição: o motor processa as vozes do mesmo jeito.

| Programa | Antes | Depois | Despertares por segundo |
|---|--:|--:|--:|
| 12 s de pausa e uma nota no fim | 2,3% | 1,9% | ~250 |
| Uma nota de 0,25 s a cada 3 s | 2,1% | 2,0% | ~250 |
| Uma nota sustentada por 12 s | 2,3% | 2,2% | ~250 |

O ganho do callback (cerca de 0,2% de um núcleo no silêncio) aparece no processo, mas é pequeno perto do resto. Os ~2% que sobram são o cliente de áudio do sistema, isto é, a miniaudio e o PulseAudio convertendo e entregando os blocos, e não o código do projeto. Os ~250 despertares por segundo são o período do dispositivo (~170 por segundo) mais as threads internas do PulseAudio e os 20 da `waitUntilFinished`.

A resolução dessa medida é de 0,14 ponto percentual (o kernel conta CPU em fatias de 10 ms), então diferenças abaixo disso entre as linhas são ruído. O número que isola o código do projeto é o microbenchmark da seção 3.3.

## 6. Como reproduzir

```bash
# Os testes desta issue
./build/tests/mus_tests --test-case="ocioso*"
ctest --test-dir build -R mus_tests_ocioso

# CPU e despertares por thread de um processo: em Linux, ler
# /proc/<pid>/task/*/stat (utime + stime) e /proc/<pid>/task/*/status
# (voluntary_ctxt_switches + nonvoluntary_ctxt_switches) no início e no fim
# de uma janela, com o compilador tocando.
```
