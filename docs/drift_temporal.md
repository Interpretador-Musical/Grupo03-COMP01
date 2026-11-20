---
title: Drift Temporal
layout: default
parent: Documentação
nav_order: 14
---

# Drift temporal em músicas finitas

Este documento registra o quanto o instante em que uma nota realmente começa a soar se afasta do instante escrito no programa, e as duas correções feitas a partir dessa medição (MAT-08, #45). Foi medido antes de corrigir: as duas correções saíram de desvios reproduzíveis, e cada uma tem um teste de regressão que falha no código anterior.

---

## 1. De onde vem o erro

No modelo batch ([ADR 001]({{ site.baseurl }}{% link decisao_modelo_batch.md %})), o instante de uma nota passa por duas conversões até virar som:

```
duração escrita (tempos) ──Playhead──▶ startTime (segundos, na Timeline) ──callback──▶ frame de áudio
         geração                                        quantização + reprodução
```

O erro total de um início foi separado em três parcelas. Cada uma é medida contra a sua própria referência, para que um defeito de geração não se confunda com um de reprodução:

| Parcela | Definição | Referência |
|---|---|---|
| **Geração** | \|`startTime` − instante exato\|, em segundos | Fórmula fechada calculada em `long double` a partir das durações racionais escritas no programa (ex.: *i* · (1/3) · 60/BPM) |
| **Quantização** | \|frame / 44 100 − `startTime`\| | É inerente a tocar a 44,1 kHz. Limitada a meio frame (~11 µs) e não se acumula, porque cada início é convertido do seu valor absoluto |
| **Reprodução** | frame em que a voz disparou − frame agendado | O frame agendado é `segundosParaFrame(startTime)` (`src/core/frames.h`). Tem de ser exatamente 0 |

## 2. Metodologia de observação

A medição é determinística e não usa dispositivo de áudio. Fica toda em `tests/test_drift.cpp`.

- **Geração.** A Timeline é gerada pelo `Playhead`, do mesmo jeito que o interpretador gera, e comparada evento a evento com a fórmula fechada. Também há um caso ponta a ponta, com programa `.mus`, `repita`, troca de `andamento` e trilhas paralelas.
- **Reprodução.** A Timeline vai para o `SpscRingBuffer` como no `main.cpp`, e o callback (`AudioEngine::renderRealTime`) é chamado **um frame por vez**. Depois de cada frame *f*, toda voz em `Attack` com `startFrame == f` é um evento que disparou em *f*, e toda voz que passou a `Idle` terminou em *f*. A lista observada é comparada com a lista agendada.

Para gerar a tabela da seção 3 (leva menos de 1 s):

```bash
cmake --build build -j
./build/tests/mus_tests --no-skip --test-case="drift: medição*"
```

A coluna "antes" foi medida com o mesmo caso sobre a `main` de `eaf014f`, antes da correção.

## 3. Medições de referência

Cada linha é uma sequência de notas iguais, com andamento constante. "Inícios fora do frame" conta os eventos cujo frame, calculado a partir do `startTime` gerado, difere do frame calculado a partir do instante exato. Ou seja, conta quantas vezes o erro de geração chegou a mover a nota para outro frame.

| Notas | Duração (tempos) | BPM | Fim (s) | Geração máx — antes | Geração máx — depois | Inícios fora do frame — antes | depois |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 100 000 | 1/10 | 120 | 5 000 | 9,4·10⁻⁹ s | 7,3·10⁻¹³ s | 0 | 0 |
| 100 000 | 1/3 | 120 | 16 667 | 2,4·10⁻⁸ s | 2,4·10⁻¹² s | 0 | 0 |
| 200 000 | 1/7 | 97 | 17 673 | 7,0·10⁻⁸ s | 3,1·10⁻¹² s | 0 | 0 |
| 1 000 000 | 1/10 | 137 | 43 796 | 5,8·10⁻⁷ s | 1,1·10⁻¹¹ s | **6 596** | 0 |
| 1 000 000 | 1/3 | 173 | 115 607 | 1,1·10⁻⁶ s | 2,3·10⁻¹¹ s | **12 864** | 0 |

Em todos os casos, a quantização máxima ficou abaixo de 0,5 frame, como esperado. A reprodução teve desvio **0** em todos os eventos observados: 1 350 eventos (1 200 notas de melodia e 75 acordes de duas notas), com troca de andamento no meio.

**Leitura.** O erro de geração crescia linearmente com o número de notas, porque era soma. Em peças de tamanho realista (minutos, milhares de notas) ele ficava três ordens de grandeza abaixo de um frame e não era audível. Com 10⁶ notas, cerca de 1% dos inícios já caíam no frame vizinho. Depois da correção, o erro de geração fica no nível do arredondamento de uma única multiplicação, e o erro total de um início se resume à quantização de meio frame.

## 4. Correções

### 4.1 Geração: soma compensada no `Playhead`

O `Playhead` já acumulava o tempo em tempos (beats), e não em segundos (MAT-04, #22). Mesmo assim, somar 0,1 ou 1/3, que não têm representação binária exata, perde alguns bits a cada nota. `Playhead::advance()` agora usa soma de **Neumaier**: o termo que cada soma descarta é recuperado exatamente e acumulado num campo de compensação, e a posição é lida como soma + compensação.

A escolha foi Neumaier e não Kahan porque Kahan falha quando a parcela é maior que o acumulado. Isso acontece sempre na primeira nota depois de uma troca de andamento, quando o acumulado volta a zero. A compensação é zerada em `setBpm()`, `reset()` e `syncToSeconds()`, que substituem a posição inteira.

A correção não muda a semântica da linguagem: a posição continua sendo a soma das durações escritas. O que muda é que essa soma agora sai correta até o último bit.

### 4.2 Reprodução: uma regra única de segundos → frame

Antes, a mesma Timeline caía em frames diferentes conforme o caminho:

| Onde | Antes | Depois |
|---|---|---|
| Início no `AudioEngine` | `llround(startTime · taxa)` | `segundosParaFrame(startTime)` |
| Duração no `AudioEngine` | `(uint64)(duration · taxa)`, truncada | `segundosParaFrame(endTime) − frame de início` |
| Início e fim no `TimelinePlayer` | `frames / taxa >= startTime`, o que equivale a arredondar para cima | `frame >= segundosParaFrame(...)` |

O efeito audível era a duração truncada: o fim de uma nota caía um frame **antes** do início da seguinte sempre que a parte fracionária da duração passava de meio frame. Isso abria um buraco de 1 frame entre notas. `src/core/frames.h` define agora uma regra única (arredondar para o mais próximo, com NaN, infinito e negativos virando 0), usada pelos dois caminhos. O fim de uma nota coincide com o início da seguinte, e a voz fica ociosa no mesmo frame em que a próxima dispara.

A duração é calculada a partir do frame **agendado**, não do frame em que a voz disparou. Assim, um disparo atrasado por underrun não estica a nota.

## 5. Tolerâncias

| O quê | Tolerância | Teste |
|---|---|---|
| Erro de geração | < 10⁻⁹ s (≈ 4·10⁻⁵ frame) para até 2·10⁵ notas, com ou sem troca de andamento | `drift: 2·10^5 notas…`, `drift: trocas de andamento…`, `drift: interpretador ponta a ponta…` |
| Quantização | ≤ 0,5 frame (≈ 11,3 µs a 44,1 kHz), sem acúmulo | `drift: 2·10^5 notas…` |
| Reprodução (início) | 0 frame | `drift: cada evento dispara exatamente no frame agendado` |
| Reprodução (fim) | 0 frame em relação a `segundosParaFrame(endTime)` | `drift: o fim de uma nota coincide com o início da seguinte` |
| Simultaneidade | Notas de acorde com `startTime` idêntico bit a bit | `drift: interpretador ponta a ponta…` |

A tolerância de geração foi escolhida para reprovar o código antigo: 2·10⁵ notas de 1/3 davam ~5·10⁻⁸ s. Com isso, ela continua uma ordem de grandeza acima do que a correção entrega, o que deixa folga para variação entre compiladores e plataformas.

## 6. Limitações

- **A medição é do callback, não do alto-falante.** A latência do dispositivo e do sistema operacional (o buffer de 256 frames, ~5,8 ms, mais o que o driver acrescentar) desloca a peça inteira por um valor constante. Ela não gera drift entre notas e não foi medida.
- **Underrun atrasa o disparo.** Se a fila esvaziar durante a reprodução, a nota dispara no primeiro frame em que o evento chegar, e não no agendado. No fluxo atual isso não acontece, porque a fila é carregada inteira antes do `start()` (SCH-05, #29). O caso fica sinalizado pela bandeira de underrun (DSP-04).
- **O primeiro sample não nulo vem um frame depois do disparo.** O envelope começa em 0 e a senoide começa na fase 0. Isso é o ataque de 5 ms, não drift: vale igual para todas as notas.
- **Troca de andamento.** Cada `andamento` converte a posição em segundos uma vez (a âncora), com erro de 1 ulp por troca. Com 500 trocas, o erro medido ficou dentro da tolerância de 10⁻⁹ s.
- **Taxa fixa de 44,1 kHz.** `segundosParaFrame` recebe a taxa como parâmetro, mas o `AudioEngine` só roda a `kSampleRate`.
- **O `TimelinePlayer` é monofônico.** Ele segue a mesma regra de frame, mas toca um evento por vez, então os testes de acorde usam o `AudioEngine`.
