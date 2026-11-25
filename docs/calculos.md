---
title: Cálculos e Cobertura
layout: default
parent: Documentação
nav_order: 9
---

# Cálculos do compilador e cobertura de testes

Este documento mapeia as contas que o pipeline batch realmente executa, da nota escrita no `.mus` até o frame de áudio, e mostra quais testes verificam cada uma (MAT-07, #40). Os casos foram escolhidos para cobrir valores normais, bordas e entradas inválidas.

Os resultados esperados dos testes novos (`tests/test_calculos.cpp`) são **literais calculados fora do projeto**, pela fórmula fechada, e não chamadas às funções testadas. Um teste que calcula o esperado com `midiToFrequency` continua passando se `midiToFrequency` estiver errada.

---

## 1. Inventário: o que é calculado e onde

| # | Cálculo | Fórmula | Onde |
|---|---|---|---|
| C1 | Nota escrita → MIDI | `(oitava + 1) · 12 + semitom`, com `#` = +1 e `b` = −1 | `scanner::analisarNota` (lexer) e `avaliarExpressao` (nota sem oitava) |
| C2 | Oitava corrente | `oitava e` trunca `⟦e⟧` em direção a zero; o resultado precisa estar entre −1 e 9 | `Interpretador::executarOitava` |
| C3 | Aritmética de expressões | `+ − * /` e menos unário em `double`; nota ± número = transposição em semitons | `Interpretador::avaliarExpressao` |
| C4 | MIDI → Hz | `440 · 2^((midi − 69) / 12)` | `midiToFrequency` (`src/core/pitch.h`) |
| C5 | Tempos → segundos | `tempos · 60 / BPM` | `Playhead::beatsToSeconds` |
| C6 | Posição do cursor | `âncora_s + tempos_desde_âncora · 60 / BPM`; a troca de BPM congela a âncora | `Playhead::now`, `setBpm`, `advance` |
| C7 | Validação de BPM, duração e altura | finito e positivo (BPM); finito e ≥ 0, sem overflow (duração); frequência finita e > 0 (altura) | `bpmValido`, `Playhead::canAdvance` (MAT-04); `executarToca` (#119) |
| C8 | Validação de volume | `0 ≤ v ≤ 1` | `Interpretador::executarVolume` (COMP-A) |
| C9 | Repetição | `repita n` executa o corpo `⌊n⌋` vezes; `n` negativo, infinito ou NaN é erro, e as voltas do programa somam no máximo 10⁷ | `Interpretador::executarRepita` |
| C10 | Duração da peça | maior `início + duração` | `Timeline::duration` |
| C11 | Segundos → frame de início | `round(início · 44100)` | `AudioEngine::renderRealTime` |
| C12 | Segundos → frames de duração | `⌊duração · 44100⌋`, com release de 220 frames (5 ms) dentro da duração | `AudioEngine::renderRealTime` |
| C13 | Limitador | primeiro o ganho `0.8 / pico`, com o pico seguido por um detector (ataque do ganho de 1 ms, retenção de 50 ms e recuperação de 150 ms) e ganho 1 abaixo de 0.8. Depois, intacto até 0.8 e compressão por `tanh` até 1.0 | `LimitadorDePico` e `softClip` (`src/audio/limiter.h`); ver [Chiados na reprodução]({{ site.baseurl }}{% link chiados.md %}) |

**Fora do pipeline batch.** `Arc`, `Hap`, `Pattern` e `subdivide` (`src/core/arc.h`, `hap.h`, `pattern.h`, `subdivision.h`) ainda não são consumidos pelo interpretador. Os testes deles continuam na suíte, mas não fazem parte desta matriz.

**Operadores booleanos.** Comparações (`== != < > <= >=`) e operadores lógicos (`e ou nao`) são avaliados, mas um booleano só pode ser guardado numa variável: a análise semântica não deixa que ele chegue à altura, à duração ou a qualquer comando. Sem `se/entao`, nenhum desses valores muda o IR, e por isso eles não entram na matriz. Quando a linguagem ganhar condicionais, entram.

---

## 2. Matriz de casos

| # | Normais | Bordas | Inválidos | Testes |
|---|---|---|---|---|
| C1 | as sete notas, `do4 = 60`, `la4 = 69` | `dob4` = si 3 e `si#4` = dó 5 atravessam a oitava | — (o lexer não aceita oitava fora de um dígito) | `test_lexer.cpp`; `test_calculos.cpp` "altura: notas escritas…" |
| C2 | `oitava 5` | `oitava 4.9` → 4; `oitava 0 - 1` → −1 (MIDI 0); `oitava 0 - 0.5` → 0 (trunca em direção a zero); `oitava 9.9` → 9; `oitava 0 - 1.9` → −1 | `oitava 10`; `oitava 0 - 2`; `oitava 1 / 0`; `oitava 0 / 0` (#124) | `test_calculos.cpp` "altura: oitava corrente…"; `test_interpreter.cpp` "oitava fora de -1 a 9…" |
| C3 | `do4 + 7` (quinta) | precedência (`2 * 3` antes de `+`), parênteses, menos unário (`- -2`), microtom (`+ 0.5`) | tipos errados são recusados antes, pela semântica | `test_calculos.cpp` "altura: aritmética…", "duração: frações…" |
| C4 | 13 notas de `la0` (27,5 Hz) a `do8` (4186,009 Hz) | MIDI 0 (8,176 Hz) e MIDI 127 (12 543,854 Hz) | `toca 1 / 0` (∞ Hz) e `toca 0 - 20000` (0 Hz) são recusados | `test_pitch.cpp`; `test_calculos.cpp` "altura: as pontas…"; `test_interpreter.cpp` "altura que não dá frequência…" |
| C5 | 60, 90, 100, 120, 128, 140 BPM | 7 BPM (8,571 s por tempo); duração 0; `1 / 3` de tempo | BPM ≤ 0, NaN ou infinito | `test_playhead.cpp`; `test_calculos.cpp` "duração: um tempo vale 60 / BPM" |
| C6 | sequência de notas | troca de BPM no meio (90 → 140); 10 000 terços de tempo sem erro acumulado | avanço negativo, NaN ou com overflow | `test_playhead.cpp` (MAT-02, MAT-04); `test_calculos.cpp` "posição: …" |
| C7 | BPM 120, duração 0,5, `do4` | duração 0 | ver tabelas MC/DC na seção 3 | `test_playhead.cpp` (MAT-04), `test_interpreter.cpp` |
| C8 | 0,5 | 0 e 1 aceitos | 1,5; −1; NaN | `test_interpreter.cpp` (COMP-A) |
| C9 | `repita 3` | `repita 2.7` → 2; `repita 0.9` → 0; `repita 0` → 0; aninhados logo abaixo do teto (99 voltas com teto de 100) | `repita 0 - 1`; `repita 1 / 0`; `repita 0 / 0`; `repita 1000000000000` (#124); aninhados acima de 10⁷ voltas (#125) | `test_calculos.cpp` "repetição: …"; `test_interpreter.cpp` "repita com quantidade…", "repita acima do teto…", "as voltas de repita aninhados…" |
| C10 | soma de durações | timeline vazia (0 s); 10 000 eventos | — | `test_playhead.cpp`; `test_representacao_intermediaria.cpp` (invariante I6) |
| C11 | 0,5 s → frame 22 050 | 1/3 s → 14 700; ¼ de frame → 0; ¾ de frame → 1 | — | `test_calculos.cpp` "reprodução: o evento começa…" |
| C12 | 0,1 s → silêncio a partir do frame 4 411 | release dentro da duração | — | `test_calculos.cpp` "reprodução: a nota se cala…" |
| C13 | amostra abaixo de 0,8 passa intacta | 0,8 exato; ±∞ | NaN → 0 | `test_limiter.cpp` (DSP-10) |

---

## 3. MC/DC onde se justifica

O MC/DC (*Modified Condition/Decision Coverage*) exige que cada condição de uma decisão composta mude, sozinha, o resultado da decisão em algum teste. Ele só se justifica onde há decisão composta numa conta crítica, e aplicamos o critério só nesses quatro pontos:

**Volume válido** (`executarVolume`): decisão `v ≥ 0 ∧ v ≤ 1`.

| Caso | `v ≥ 0` | `v ≤ 1` | Aceita? | Mostra o efeito de |
|---|---|---|---|---|
| `volume 0.5` | V | V | sim | — |
| `volume 1 - 2` | **F** | V | não | `v ≥ 0` |
| `volume 1.5` | V | **F** | não | `v ≤ 1` |

**BPM válido** (`bpmValido`): decisão `finito(b) ∧ b > 0`.

| Caso | `finito` | `> 0` | Aceita? | Mostra o efeito de |
|---|---|---|---|---|
| 140 | V | V | sim | — |
| `+∞` | **F** | V | não | `finito` |
| 0 | V | **F** | não | `> 0` |

**Duração válida** (`canAdvance`, primeira decisão): `finito(d) ∧ d ≥ 0`.

| Caso | `finito` | `≥ 0` | Aceita? | Mostra o efeito de |
|---|---|---|---|---|
| 0,5 | V | V | sim | — |
| `+∞` | **F** | V | não | `finito` |
| −1 | V | **F** | não | `≥ 0` |

**Altura válida** (`executarToca`, #119): decisão `finito(f) ∧ f > 0`, sobre a frequência calculada.

| Caso | `finito` | `> 0` | Aceita? | Mostra o efeito de |
|---|---|---|---|---|
| `toca do4` | V | V | sim | — |
| `toca 1 / 0` | **F** | V | não | `finito` |
| `toca 0 - 20000` (subfluxo para 0 Hz) | V | **F** | não | `> 0` |

Todos os casos das quatro tabelas estão em `test_interpreter.cpp` e `test_playhead.cpp`. A segunda decisão do `canAdvance` testa se cada resultado intermediário (posição total, instante atual, fim do evento) continua finito. Ela é uma defesa contra overflow, e os seus termos não são independentes: se a duração e o cursor são finitos, quase todos os outros também são. Por isso o MC/DC não se aplica a ela, e a cobertura de ramos mostra os três termos que nenhum teste consegue tornar falsos sozinhos.

---

## 4. Cobertura medida

Medição com `gcov` (GCC 13) sobre a suíte inteira, em 02/11/2026:

| Arquivo | Linhas | Ramos tomados | O que não é coberto |
|---|---|---|---|
| `src/core/playhead.cpp` | 100% | 85% | os três termos dependentes do `canAdvance` (seção 3) |
| `src/core/timeline_emit.cpp` | 100% | 58% | ramos internos gerados pelo compilador para `std::string` e `snprintf` |
| `src/interpreter/interpreter.cpp` | 80% | 48% | comparações e `ou` (sem efeito no IR, seção 1); os `return` de propagação de erro depois de uma falha anterior |
| `src/audio/audio_engine.cpp` | 64% | 40% | abertura do dispositivo e modo `--test-tone`, que exigem placa de som |

Para reproduzir:

```bash
cmake -S . -B build-cov -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="--coverage -O0" -DCMAKE_EXE_LINKER_FLAGS="--coverage"
cmake --build build-cov --target mus_tests -j
./build-cov/tests/mus_tests
cd build-cov && gcov -b -o CMakeFiles/mus_core.dir/src/core CMakeFiles/mus_core.dir/src/core/playhead.cpp.o
```

---

## 5. Lacuna conhecida

**Testes antigos com esperado calculado pela própria função.** Alguns casos de `test_interpreter.cpp` comparam com `midiToFrequency(67)` ou `beatsToSeconds(1.0)`. Eles continuam úteis como teste de integração, e os valores absolutos agora estão garantidos pelos literais de `test_calculos.cpp`.

---

## 6. Validação complementar de arcos e precisão temporal — MAT-12 (#65)

A MAT-12 complementa a matriz da MAT-07 com testes de
independência de condições, arcos semiabertos e limites de
representabilidade em ponto flutuante.

### 6.1. Arcos temporais

Os testes de `tests/test_arc.cpp` verificam as operações
`contains`, `sect`, `cycles` e `divide`.

Para `Arc::contains(t)`, a decisão é:

`(t >= begin) && (t < end)`

Considerando o arco `[1, 2)`:

| Caso | Condição inferior | Condição superior | Resultado |
|---|---|---|---|
| t = 1.5 | V | V | V |
| t = 0.5 | F | V | F |
| t = 2.0 | V | F | F |

Os pares de casos demonstram a influência independente
de cada condição sobre a decisão.

Para `Arc::divide(n)`, a condição de rejeição é:

`(n == 0) || isEmpty()`

| Caso | n == 0 | isEmpty() | Retorna vazio |
|---|---|---|---|
| Arco válido, n = 2 | F | F | Não |
| Arco válido, n = 0 | V | F | Sim |
| Arco vazio, n = 2 | F | V | Sim |

Os testes demonstram MC/DC das duas condições.

Os pares MC/DC de contains e divide são cobertos pelos testes preexistentes em tests/test_arc.cpp. A MAT-12 documenta a matriz de condições e identifica esses casos, sem duplicar suas verificações.

### 6.2. Representabilidade temporal

Os testes de `tests/test_playhead.cpp` verificam:

- Duração positiva que sofre underflow na conversão para segundos.
- Duração mínima normal ainda representável.
- Duração zero em instante absoluto elevado.
- Incremento positivo que avança na origem, mas não altera
  um instante elevado por falta de resolução do `double`.


### 6.3. Decisão de avanço positivo

A decisão final de `Playhead::canAdvance()` é:

`A && (B || C)`

Onde:

- A: `beats > 0.0`
- B: `duracaoSegundos <= 0.0`
- C: `novoTempo <= tempoAtual`

A matriz MC/DC utiliza os seguintes cenários:

| Cenário | A | B | C | Rejeita |
|---|---|---|---|---|
| Duração pequena na origem | V | F | F | Não |
| Duração zero em instante elevado | F | V | V | Não |
| Duração pequena em instante elevado | V | F | V | Sim |
| Duração subnormal com arredondamento | V | V | F | Sim |

**Influência de A — MC/DC com mascaramento:**

Comparando a duração zero com a duração pequena em um
instante elevado, A muda de falsa para verdadeira.

Nesse par, B também muda: com duração zero,
`duracaoSegundos <= 0.0` é verdadeira.

Entretanto, C permanece verdadeira e mascara B
na subexpressão `B || C`, que permanece verdadeira
nos dois cenários.

Assim, a alteração de A modifica o resultado final
da decisão. O par demonstra MC/DC com mascaramento,
não independência de causa única.

**Independência de B:**

Comparando uma duração pequena e representável na origem
com o cenário subnormal, A permanece verdadeira e C
permanece falsa.

No cenário subnormal, a conversão da duração para segundos
produz zero, embora a posição absoluta calculada avance
devido ao arredondamento de ponto flutuante.

Assim, alterar B modifica o resultado da decisão.

**Independência de C:**

Comparando a duração pequena na origem com a mesma duração
em um instante elevado, A permanece verdadeira e B
permanece falsa.

A diferença de resolução temporal altera C e,
consequentemente, o resultado da decisão.

Os testes correspondentes estão em `tests/test_playhead.cpp`:

- `MAT-12: MC/DC de avanco positivo e resolucao temporal`
- `MAT-12: MC/DC da condicao B em avanco subnormal`

A decisão possui pares MC/DC demonstrados para as três condições:
independência de causa única para B e C e MC/DC com mascaramento
para A. Não foi necessário modificar a implementação.

**Dependências de ponto flutuante e portabilidade:**

Dois cenários da MAT-12 dependem da preservação de números
subnormais em operações de ponto flutuante:

- O subcase `duracao minima normal ainda pode ser representada`
  utiliza `DBL_MIN * 0.5` na conversão de beats para segundos.
  O resultado é subnormal. Se o ambiente descartar esses
  valores, o avanço poderá ser recusado.

- O teste `MAT-12: MC/DC da condicao B em avanco subnormal`
  depende de operações com valores subnormais e do
  arredondamento para o valor representável mais próximo,
  com desempate para o par, para distinguir B de C.

Esses testes pressupõem que o ambiente preserve subnormais
e utilize o arredondamento esperado. A compatibilidade
deve ser verificada na CI em Ubuntu e macOS.

A opção `-ffast-math` não deve ser habilitada para esses
testes sem reavaliar suas hipóteses numéricas.


### 6.4. Escopo e rastreabilidade com a AST

A MAT-12 (#65) complementa a cobertura do núcleo matemático
e das decisões de avanço temporal, sem alterar o código
de produção.

Os testes de `Arc::contains()` e `Arc::divide()` verificam
as condições das operações sobre intervalos temporais.
Os testes de `Playhead::canAdvance()` verificam limites
de representabilidade, precisão e avanço temporal.

No pipeline atual, o interpretador utiliza `Playhead`
para calcular o posicionamento dos eventos gerados a
partir da AST.

Essa integração é exercitada por testes existentes em
`tests/test_interpreter.cpp`, incluindo:

- `repita N vezes gera N eventos avançando o playhead`;
- `pausa gera um evento silencioso que avança o tempo`;
- `MAT-04: timeline com pausas e mudanca de BPM`.

Por outro lado, `Arc`, `Hap` e `subdivide` ainda não
participam diretamente do pipeline batch do interpretador.
Sua cobertura MC/DC permanece no nível unitário.

Portanto, a MAT-12 demonstra a cobertura das decisões
matemáticas selecionadas e identifica testes existentes
da integração AST → Interpretador → Timeline, mas não
comprova cobertura MC/DC de ponta a ponta desse fluxo.

A integração futura de `Arc`, `Hap` e `subdivide` ao
interpretador exigirá testes adicionais. A cobertura
atual não deve ser interpretada como validação dessa
integração ainda inexistente.
