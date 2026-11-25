---
title: Chiados na Reprodução
layout: default
parent: Documentação
nav_order: 19
---

# Chiados na reprodução (BUG-AUDIO, #109)

Este documento registra a investigação da BUG-AUDIO (#109), que relatava chiados ocasionais na reprodução musical. Ele mostra como o defeito foi reproduzido, que hipóteses foram medidas e descartadas, qual foi a causa confirmada e o que mudou no motor de áudio.

## 1. Resumo

| Pergunta | Resposta |
|---|---|
| O defeito foi reproduzido? | Sim, fora do dispositivo, em qualquer programa cuja mixagem passe de 0.8. Isso acontece com uma nota no volume padrão (1), com acordes e com o `exemplos/polirritmia.mus` do próprio repositório. |
| Qual é a causa? | O master bus tinha só o `softClip` (DSP-10, #57), uma curva sem memória. Cada nota vai até volume 1, e as vozes se somam sem atenuação. Num acorde de três notas, a soma chega a 3.0, 39% das amostras saem da `tanh` achatadas perto de 1.0, e a senoide vira quase uma onda quadrada. Os harmônicos agudos dessa quina são o chiado. |
| O que foi corrigido? | Um limitador de pico (`LimitadorDePico`, `src/audio/limiter.h`) entrou antes do `softClip`. Ele baixa o ganho da mixagem inteira em vez de deformar a onda. |
| Qual foi o efeito? | No `polirritmia.mus`, a energia acima de 4 kHz caiu de -49.6 dB para -89.0 dB, o mesmo nível do `arpejo.mus`, que nunca passou do limiar. As 94 quinas na onda caíram para 0. |
| Custo no callback | +0.6 ms por 10 s de áudio com 32 vozes (24.5 → 25.1 ms, mediana de 6 execuções). Continua em 0.25% de um núcleo. |
| O que não foi resolvido | Ver a [seção 7](#7-limitações). O som que sai da placa não foi medido, e o relato original não registrou qual programa chiava. |

## 2. Como reproduzir

O executável `mus_chiados` compila cada programa pelo pipeline inteiro e renderiza a mixagem fora do dispositivo. Ele usa o mesmo `renderRealTime` que o callback do miniaudio chama, em blocos de 256 frames. A mesma mixagem passa pelas duas cadeias de master bus: a antiga (só o `softClip`) e a nova (limitador e depois `softClip`). Assim o antes e o depois saem do mesmo build.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target mus_chiados
./build/tests/mus_chiados                       # casos fixos + exemplos/
./build/tests/mus_chiados meu_programa.mus      # outros programas
./build/tests/mus_chiados --wav chiados/        # grava cada caso antes e depois em WAV
```

Com `--wav`, cada caso gera `<caso>_antes.wav` e `<caso>_depois.wav`, em 16 bits e 44,1 kHz. São os arquivos para ouvir a diferença. O mais claro é o `exemplos_polirritmia_mus`.

**Ambiente das medições:** macOS 27.0.1 num MacBook Air com Apple Silicon (arm64), Apple Clang, build `Release`, em 24/11/2026. A renderização offline não depende do dispositivo. Para a seção 3, o dispositivo foi aberto de verdade: Core Audio, alto-falantes internos, com o motor pedindo 44,1 kHz mono e o Core Audio rodando internamente a 48 kHz estéreo, com 3 períodos de 256 frames.

### 2.1 Métricas

- **Energia acima de 4 kHz:** a energia que passa de um passa-altas de 8ª ordem (quatro biquads RBJ em cascata, em 4 kHz), dividida pela energia total, em dB. As notas dos casos ficam abaixo de 1 kHz, então o que passa do filtro é distorção ou quina na onda. Com 2ª ordem, o próprio sol4 vazaria a -40 dB e esconderia a medida. Os samples de percussão têm agudo de verdade, e neles essa métrica não separa defeito de timbre.
- **Quinas:** amostras em que a segunda diferença `|x[n] - 2x[n-1] + x[n-2]|` passa de 0.05. Uma senoide de amplitude 1 a 1 kHz não passa de 0.02. Um corte seco ou um achatamento aparece aqui.
- **Acima de 0.8:** fração das amostras da mixagem crua (antes do master bus) acima do limiar do `softClip`. É quanto do sinal passa pela parte curva da `tanh`.

## 3. Hipóteses investigadas

A issue pedia para considerar várias hipóteses antes de afirmar uma causa. Cada uma foi medida.

| Hipótese | Como foi medida | Resultado | Veredito |
|---|---|---|---|
| Clipping e saturação | Fração da mixagem acima de 0.8 e energia acima de 4 kHz, com e sem o limitador (seção 5) | De 26% a 50% das amostras na `tanh` nos casos com volume 1 ou acorde. Tirar a saturação reduz o agudo de 22 a 39 dB nesses casos | **Causa confirmada** |
| Transições abruptas e envelopes | Quinas na saída de melodias e staccato a 1200 BPM | Nenhuma quina. Ataque e release de 5 ms, e a correção das notas curtas da #123 (PR #148) | Descartada |
| Underruns | Tempo de render de 10 s com 32 vozes (`perfil: renderizar 10 s…`) | 25 ms para 10 s de áudio: o callback usa 0.25% de um núcleo. A Timeline inteira entra na fila antes do `start()` | Descartada nesta máquina |
| Reciclagem de vozes | 32 notas sustentadas e mais 8 roubando voz (caso "roubo de voz") | Energia acima de 4 kHz em -87.1 dB e nenhuma quina, antes e depois | Descartada |
| Ring buffer e sincronização | Fila SPSC carregada por inteiro antes do áudio (SCH-05, #29) e o TSan no CI com o áudio tocando (SCH-08, #46) | Sem corrida e sem produtor concorrente durante a reprodução | Descartada |
| Corte no fim da reprodução | `framesRenderizados()` no instante do `stop()`, com o dispositivo real e com o nulo | O motor já tinha renderizado de 35 a 50 ms além do fim do programa, e a última nota tinha terminado o release. O que o `stop()` descarta é silêncio | Descartada |
| Bordas dos samples | Primeira e última amostra, e segunda diferença, dos 85 WAVs de `samples/` decodificados | Todos terminam perto de zero. As segundas diferenças altas já estão no próprio arquivo de percussão (são ataques do instrumento) | Descartada |
| Reamostragem para 48 kHz | Configuração do dispositivo | O miniaudio converte 44,1 → 48 kHz com reamostrador linear e passa-baixas de 4ª ordem (`MA_DEFAULT_RESAMPLER_LPF_ORDER`) | Não medida (seção 7) |

## 4. Causa

Até aqui, o master bus era só o `softClip` da DSP-10: amostras até 0.8 passam intactas, e acima disso a sobra é comprimida por `tanh` até 1.0. A curva não tem quina no limiar, e por isso não gera estalo num pico ocasional. Mas ela não tem memória. Cada amostra é dobrada pela mesma `tanh`, não importa o que veio antes, e o quanto ela dobra depende só de quanto a amostra passa de 0.8.

O problema está no ganho que chega a ela:

- **O volume vai até 1 por nota, e o padrão da linguagem é 1.** Uma senoide de volume 1 vai até 1.0, então 37% das amostras de uma nota sozinha já passam pela `tanh` (caso "nota no volume 1").
- **As vozes se somam sem atenuação.** Três notas de volume 1 chegam a 2.995. Acima de uns 1.3, `tanh((x - 0.8) / 0.2)` já está praticamente em 1, e a curva vira um corte. No acorde de três notas, 39% das amostras da mixagem passam de 1.0 e saem coladas em ~1.0.

A senoide achatada no topo é uma onda quase quadrada. Os harmônicos ímpares dela vão até o fim da banda e dão o chiado. As 94 quinas do `polirritmia.mus` estão nos joelhos dessa onda.

**Por que "ocasional".** O defeito só aparece nos trechos em que a mixagem passa de 0.8: acordes, grupos com `,` (CMP-07) e notas no volume padrão. Uma melodia com `volume 0.8`, como o `arpejo.mus`, nunca chega ao limiar e sai limpa (-89.2 dB). Por isso o mesmo programa pode soar bem numa parte e chiar em outra.

**Histórico.** Antes da DSP-10 (PR #101, mergeado em 30/10, mesmo dia em que a #109 foi aberta), o limite era um corte seco em ±1.0 dentro do `renderRealTime`, ainda pior: a quina aparecia já em 1.0, sem curva. O `softClip` tirou a quina do limiar, mas não a saturação.

## 5. Correção

O master bus passa a ter duas etapas, nesta ordem (`AudioEngine::render`):

1. **`LimitadorDePico`**: baixa o ganho da mixagem inteira até o pico caber em 0.8, sem deformar a onda.
2. **`softClip`**: continua como rede de segurança. Troca NaN por silêncio e infinito por 1.0, e dobra o pequeno estouro que o limitador deixa passar no ataque.

O limitador segue o pico do sinal com um detector e calcula o ganho que traz esse pico até `kLimitadorTeto` (0.8, o limiar do `softClip`). Como o ganho muda devagar perto do período das notas, a senoide sai inteira, só mais baixa.

| Parâmetro | Valor | Por quê |
|---|---|---|
| Teto | 0.8 | O limiar do `softClip`: com o ganho já ajustado, nenhuma amostra sustentada chega à parte curva da `tanh`. |
| Detector | Ataque instantâneo | Se a amostra passa do pico guardado, o pico passa a ser ela. |
| Ataque do ganho | 1 ms | O ganho desce até o alvo por uma exponencial, não num degrau. A versão com degrau foi medida e descartada (abaixo). |
| Retenção | 50 ms | O pico guardado não cai entre dois ciclos da mesma nota (o dó1 tem período de 31 ms). A retenção é renovada sempre que um ciclo passa do teto e chega a 98% do pico guardado. Sem isso, num tom sustentado ela venceria a cada 50 ms, e o ganho subiria e voltaria. Medido com uma senoide de amplitude 2: sem a renovação, o ganho ondula 6.1% no dó1, 2.0% no dó2 e 0.4% em 1 kHz; com ela, 0.00%. |
| Recuperação | 150 ms, exponencial | Depois da retenção, o ganho volta a 1 sem degrau de volume. |
| Abaixo do teto | Ganho exatamente 1 | O sinal passa intacto, bit a bit. O pico guardado nunca desce abaixo do teto, o que também evita números subnormais no silêncio. |

O limitador não aloca nem bloqueia, e custa O(1) por amostra. O estado é reiniciado em `start()`, antes de o callback começar.

**Alternativas consideradas:**

- **Só um ganho fixo no master bus** (por exemplo, 0.25). Deixaria todo programa mais baixo e ainda saturaria com mais vozes: com 32 vozes possíveis, nenhum ganho fixo cobre o pior caso sem deixar uma nota sozinha quase inaudível.
- **Dividir pelo número de vozes ativas.** O ganho daria um salto a cada nota que entra ou sai, e esse salto é outro clique.
- **Limitador com degrau de ganho**, ou seja, o mesmo detector sem o ataque de 1 ms. Foi a primeira versão, e foi medida. Nos acordes muito altos, ele já tirava quase todo o agudo. Mas no "acorde no volume 0.33", que só passa de 0.8 nas cristas, ficou **pior** que o `softClip` sozinho: -67.1 dB contra -81.6 dB. As frequências do acorde não são múltiplas exatas umas das outras, então o pico da soma oscila devagar. Cada pico novo baixava o ganho num degrau, no meio da onda, e o degrau gera agudo. O ataque de 1 ms levou esse caso para -90.5 dB.
- **Look-ahead**: atrasar a saída alguns milissegundos para baixar o ganho antes do pico chegar. Evitaria também o estouro no ataque, mas atrasaria todo o áudio e mudaria o frame em que cada nota aparece em `render()`, que vários testes conferem. O estouro que sobra é pequeno (seção 7).

## 6. Validação

Saída do `mus_chiados` no ambiente da seção 2. "Antes" é a mixagem passando só pelo `softClip`; "depois" é a mesma mixagem passando pelo limitador e pelo `softClip`.

| Programa | Vozes | Pico da mixagem | Acima de 0.8 | > 4 kHz antes (dB) | > 4 kHz depois (dB) | Quinas antes | Quinas depois | Pico antes | Pico depois |
|---|--:|--:|--:|--:|--:|--:|--:|--:|--:|
| nota no volume 1 | 1 | 1.000 | 36.8% | -64.0 | -88.1 | 0 | 0 | 0.952 | 0.937 |
| acorde no volume 1 | 3 | 2.995 | 50.1% | -47.1 | -76.5 | 94 | 0 | 1.000 | 0.999 |
| acorde no volume 0.33 | 3 | 0.988 | 4.2% | -81.6 | -90.5 | 0 | 0 | 0.947 | 0.908 |
| acorde de 6 no volume 0.5 | 6 | 2.470 | 35.5% | -52.6 | -75.0 | 2 | 0 | 1.000 | 1.000 |
| graves no volume 1 | 2 | 1.760 | 42.1% | -102.7 | -99.3 | 0 | 0 | 1.000 | 0.981 |
| staccato a 1200 BPM | 1 | 1.000 | 26.2% | -51.5 | -77.5 | 0 | 0 | 0.952 | 0.919 |
| roubo de voz (33 notas) | 32 | 0.299 | 0.0% | -87.1 | -87.1 | 0 | 0 | 0.299 | 0.299 |
| bateria (samples) | 2 | 0.541 | 0.0% | -37.6 | -37.6 | 0 | 0 | 0.541 | 0.541 |
| exemplos/arpejo.mus | 1 | 0.800 | 0.0% | -89.2 | -89.2 | 0 | 0 | 0.800 | 0.800 |
| exemplos/escala.mus | 1 | 1.000 | 39.6% | -64.8 | -90.3 | 0 | 0 | 0.952 | 0.917 |
| exemplos/polirritmia.mus | 3 | 2.995 | 42.2% | -49.6 | -89.0 | 94 | 0 | 1.000 | 0.980 |

**Como ler:**

- Nos casos que nunca passam de 0.8 (arpejo, roubo de voz, bateria), antes e depois são idênticos. Abaixo do teto, o limitador não muda nenhuma amostra.
- Nos casos que saturavam de verdade (volume 1 ou acorde alto), o agudo caiu de 22 a 39 dB, e as quinas sumiram. No acorde de volume 0.33, que só passa do limiar nas cristas, caiu 9 dB.
- Nos dois acordes altos, o que sobra de agudo (-76.5 e -75.0 dB) está todo nos primeiros 50 ms, no ataque. Medido em janelas de 50 ms, do segundo trecho em diante o agudo fica abaixo de -96 dB. É o estouro do ataque, que o `softClip` dobra (seção 7). Os picos de saída perto de 1.0 nesses casos vêm do mesmo estouro.
- Nos graves, a métrica de 4 kHz não serve: os harmônicos de 33 e 65 Hz que a `tanh` cria ficam quase todos abaixo do filtro, e os dois números estão perto do piso. O que mostra que o grave não é deformado é o teste de tom sustentado: com o dó1 acima do teto, o ganho fica constante (variação medida de 0.00%).

### 6.1 Testes de regressão

Em `tests/test_limiter.cpp`, com o mesmo filtro da métrica:

| Teste | O que garante | Sem o limitador |
|---|---|---|
| `master bus: acorde de três notas no volume 1 sai sem chiado` | Agudo < -70 dB no acorde do `polirritmia.mus` | Falha: -47.1 dB |
| `master bus: acorde que só passa do limiar nas cristas não piora` | Agudo < -85 dB no acorde de volume 0.33 | Falha: -81.6 dB (com degrau de ganho, -67.1 dB) |
| `master bus: uma nota no volume 1 sai sem chiado` | Agudo < -80 dB numa nota de volume 1 | Falha: -64.0 dB |
| `limitador: abaixo do teto o sinal passa intacto, bit a bit` | O sinal normal não é colorido | — |
| `limitador: tom sustentado acima do teto sai senoidal, só mais baixo` | Ganho constante em 33 Hz, 262 Hz e 1 kHz (variação abaixo de 0.002), e pico no teto | Sem a renovação da retenção, falha no dó1 (variação de 0.026) e no dó4 (0.004) |
| `limitador: o ganho volta a 1 depois do trecho alto` | Recuperação e volta ao sinal intacto, com o trecho seguinte em 0.5, 0.79 e no próprio teto | Com a renovação também abaixo do teto, falha em 0.79 e 0.8 (ganho preso em até 0.98) |
| `limitador: NaN e infinito passam sem envenenar o estado` | Um infinito não deixa o resto da música mudo | — |
| `limitador: reiniciar devolve o ganho a 1` | Estado limpo a cada `start()` | — |

Os testes de master bus da DSP-10 continuam passando sem mudança: nenhuma saída acima de 1, sinal absurdo limitado mas não mudo, e ganho de 0.5 sem atenuação.

Os limites dos três testes de regressão têm uns 5 dB de folga sobre o que o motor corrigido mede. A folga é para não falharem por diferença de compilador. Os três falham com o `softClip` sozinho, o que foi conferido desligando o limitador em `render()`.

## 7. Limitações

- **Estouro no ataque de notas altas.** Com o ataque de 1 ms, o ganho chega atrasado quando o nível sobe rápido, e as primeiras amostras passam do teto. O `softClip` dobra esse estouro. Num acorde de três notas no volume 1, esse é todo o agudo que sobra (-76.5 dB, nos primeiros 50 ms). Um look-ahead resolveria, ao preço de atrasar a saída (seção 5).
- **Volume.** Uma nota sozinha no volume 1 sai com pico 0.8, e não mais ~0.95 (-1.5 dB). Num acorde alto, o ganho baixa para toda a mixagem junto e volta em uns 200 ms depois que o acorde acaba. O volume relativo entre as notas do mesmo trecho não muda.
- **Folga de até 2% logo acima do teto.** Depois de um trecho mais alto, um tom sustentado que ainda passa de 0.8 pode ficar com o ganho até 2% (0.18 dB) abaixo do necessário, porque a retenção é renovada a partir de 98% do pico guardado. O ganho fica constante, então não há distorção. Abaixo do teto isso não acontece: o ganho volta a 1.
- **Medido no sinal, não na placa.** Toda a medição é sobre o que o motor entrega ao miniaudio. A conversão para 48 kHz e o que sai do alto-falante não foram medidos. Um underrun causado pelo sistema operacional, sob carga de outros processos, também não foi reproduzido (a mitigação é a da DSP-04, #24).
- **O relato original não tem programa.** A issue não registrou qual `.mus`, BPM ou volume chiava, então não dá para afirmar que este é exatamente o som que o grupo ouviu. O que dá para afirmar é que os exemplos do próprio repositório no volume padrão apresentam esse chiado, que ele tem causa medida e que a correção o remove. A confirmação de ouvido fica com os WAVs de antes e depois gerados pelo `mus_chiados --wav`.
