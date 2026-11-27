---
title: Teste de Estresse
layout: default
parent: Documentação
nav_order: 11
---

# Teste de estresse do pipeline batch

Este documento registra o teste de estresse do compilador (INT-07, #43): programas `.mus` finitos, com BPM alto e muitos eventos, passando pelo pipeline inteiro, do lexer até a saída do motor de áudio. O objetivo é saber **até onde o compilador aguenta** e **o que quebra depois disso**.

Todas as cargas são programas finitos, compilados inteiros antes de a primeira amostra ser gerada (modelo batch, [ADR 001]({{ site.baseurl }}{% link decisao_modelo_batch.md %})). Nenhum teste pressupõe padrão infinito ou edição em tempo real.

## 1. Resumo

| Pergunta | Resposta |
|---|---|
| O compilador trava em algum cenário aceito? | Não. |
| Algum evento se perde entre o `.mus` e a fila de áudio? | Não. A contagem bate com a fórmula fechada em todas as cargas originalmente medidas, até 10 milhões de eventos. Desde 19/11/2026, o teto permitido é de 5 milhões de eventos, e programas que o ultrapassam são recusados (#125). |
| O motor de áudio dá conta da densidade? | Sim. Nas medições originais em macOS Release, gerou áudio cerca de **2 200 vezes** mais rápido que o tempo real. Após a correção da DSP #123, medições adicionais em Release confirmaram o funcionamento sem notas mudas, atenuadas ou falsos underruns nos cenários de 2400 e 6000 BPM. Os resultados foram obtidos em ambientes diferentes e não devem ser comparados diretamente. |
| Até onde vai o envelope aceito? | Notas de **2,5 ms** ou mais: até **6000 BPM** com figura de 0.25 tempo (400 eventos por segundo), conforme validado após a correção da #123. No máximo **5 000 000 eventos** e **10 000 000 voltas de `repita`** por programa. |
| O que quebra fora dele? | O problema de notas abaixo de 10 ms atenuadas ou mudas foi corrigido pela #123, com ataque e release adaptativos. Notas de 1 e 2 frames possuem tratamento específico para produzir sinal, mas a ausência de cliques nesses casos extremos ainda não foi comprovada. Os problemas de valores não finitos em `repita` e `oitava` (#124) e da ausência de limite de eventos (#125) foram corrigidos em 19/11/2026. Programas que violam esses limites são recusados com erro de execução (ver seções 6.2 e 6.3). A memória cresce cerca de 100 bytes por evento. |

## 2. Como reproduzir

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Afirmações (contagem, duração, saída limpa): roda no CTest e no CI
ctest --test-dir build -R estresse --output-on-failure

# Medições de tempo: imprime as tabelas da seção 4
./build/tests/mus_estresse
```

O que cada peça faz:

| Arquivo | Papel |
|---|---|
| `tests/estresse/cargas.h` | Geradores das cargas, cada uma com eventos, notas e duração calculados por fórmula fechada. Também o pipeline (`compilar`) e a reprodução fora do dispositivo (`reproduzirOffline`). |
| `tests/test_estresse.cpp` | Suíte doctest `estresse`, entrada `mus_tests_estresse` do CTest. Afirma propriedades, nunca tempo. |
| `tests/estresse/medir_estresse.cpp` | Executável `mus_estresse`: mede o tempo de cada fase. Fica fora do CTest, porque tempo de relógio depende da máquina. |
| `tests/estresse/*.mus` | Quatro programas para o executável `compilador` de verdade (`estresse_cli_*` no CTest), com a conta esperada no comentário do topo: três cargas aceitas e `acima_do_teto.mus`, que precisa ser recusado (#125). |
| `tests/estresse/conta_eventos.cmake` | Roda `compilador --emit-timeline` e confere o número de linhas contra o esperado. |

**Como a reprodução é medida sem placa de som.** `reproduzirOffline` carrega a Timeline na fila SPSC, como o `main.cpp` faz, e chama `AudioEngine::render` em blocos de 256 frames, os mesmos que o callback do miniaudio pediria. Passa pela síntese e pelo limitador do master bus. Depois examina a saída:

- **nota muda:** o pico dentro da janela da nota ficou abaixo de 1% do volume pedido;
- **nota atenuada:** o pico ficou abaixo de 50% do volume;
- **underrun:** a bandeira que o motor levanta quando a fila acaba antes da duração esperada. Com a fila carregada inteira antes de tocar, qualquer underrun é falso positivo;
- **pico**, **NaN/infinito** na saída, e **som depois do fim esperado**.

A linguagem não tem notas simultâneas, então a janela de uma nota não contém o som de outra.

## 3. Cargas

| Carga | Programa | Eventos | Duração |
|---|---|---|---|
| `cargaGrade(bpm, f, s)` | `repita N vezes { toca do4 por f  toca sol4 por f  pausa por f }` | 3N (2N notas) | N · 3f · 60/bpm |
| `cargaAninhada(bpm, a, b, f)` | dois `repita` aninhados, `a × b` notas | a · b | a · b · f · 60/bpm |
| `cargaPlana(bpm, n, f)` | `n` linhas de `toca`, sem `repita` | n | n · f · 60/bpm |
| `cargaMista(v)` | variável, transposição por expressão, `oitava`, `volume`, troca de andamento dentro do laço, três níveis de `repita` | 25v (17v notas) | 0.95 s · v |

A grade varia o andamento (60 a 6000 BPM) e a figura, e com isso a densidade, sem mudar a forma do programa. A aninhada gera volume com um programa pequeno e exercita o interpretador. A plana gera volume com um programa grande e exercita o lexer e o parser.

## 4. Resultados

**Ambiente:** Apple M4, 16 GB, macOS 27.0.1, Apple clang 21.0.0, CMake 4.3.3, Flex 2.6.4, Bison 2.3. Build `Release`. Medido em 02/11/2026, commit-base `817fe71`. Os tempos de uma execução variam algumas dezenas de por cento. O que importa é a ordem de grandeza.

### 4.1 Andamento e densidade (com reprodução)

Grade com figura de 0.25 tempo, cerca de 10 s de música cada.

| BPM | Menor nota | Eventos | Esperado | Compilação | Render | Fator | Mudas | Atenuadas | Underruns | Pico |
|--:|--:|--:|--:|--:|--:|--:|--:|--:|--:|--:|
| 60 | 250 ms | 39 | 39 | 0.02 ms | 4.3 ms | 2273× | 0 | 0 | 0 | 0.952 |
| 120 | 125 ms | 81 | 81 | 0.01 ms | 4.5 ms | 2232× | 0 | 0 | 0 | 0.952 |
| 240 | 62.5 ms | 159 | 159 | 0.01 ms | 4.5 ms | 2227× | 0 | 0 | 0 | 0.952 |
| 600 | 25 ms | 399 | 399 | 0.01 ms | 4.5 ms | 2196× | 0 | 0 | 0 | 0.952 |
| 1000 | 15 ms | 666 | 666 | 0.02 ms | 4.5 ms | 2203× | 0 | 0 | 0 | 0.952 |
| **1500** | **10 ms** | 999 | 999 | 0.02 ms | 4.5 ms | 2242× | **0** | **0** | **0** | 0.952 |
| 2400 | 6.25 ms | 1599 | 1599 | 0.02 ms | 3.3 ms | 3034× | 0 | **1066 (todas)** | **2** | 0.250 |
| 6000 | 2.5 ms | 3999 | 3999 | 0.06 ms | 2.7 ms | 3777× | **2666 (todas)** | 0 | **2** | 0.000 |

"Compilação" soma lexer, parser, semântica e interpretação. "Fator" é a duração da música dividida pelo tempo de render.

| Carga sustentada | Eventos | Esperado | Duração | Render | Fator | Mudas | Atenuadas | Underruns | Pico |
|---|--:|--:|--:|--:|--:|--:|--:|--:|--:|
| grade 1500 BPM, 60 s | 6000 | 6000 | 60 s | 25 ms | 2383× | 0 | 0 | 0 | 0.952 |
| mista, 100 voltas | 2500 | 2500 | 95 s | 39 ms | 2411× | 0 | 0 | 0 | 0.800 |

O pico de 0.952 é o limitador do master bus trabalhando sobre uma nota de volume 1.0 (DSP-10, #57). Na mista, o volume máximo do programa é 0.8. Depois da BUG-AUDIO (#109), o master bus tem um limitador de pico antes do `softClip`, e o `mus_estresse` mede 0.917 nessas grades. O valor sai do estouro no ataque de cada nota, porque o sustentado fica em 0.8 ([Chiados na reprodução]({{ site.baseurl }}{% link chiados.md %})). As tabelas desta seção são a medição original.

### 4.2 Volume (só compilação)

Sem render: um milhão de eventos a 600 BPM são quase 7 horas de áudio.

| Carga | Eventos | Esperado | Frontend | Semântica | Interpretação |
|---|--:|--:|--:|--:|--:|
| aninhada 100 × 1000 | 100 000 | 100 000 | 0.01 ms | 0.00 ms | 2.3 ms |
| aninhada 1000 × 1000 | 1 000 000 | 1 000 000 | 0.00 ms | 0.00 ms | 24.5 ms |
| aninhada 10000 × 1000 | 10 000 000 | 10 000 000 | 0.01 ms | 0.00 ms | 454.5 ms |
| plana 10 000 linhas | 10 000 | 10 000 | 1.65 ms | 0.05 ms | 0.23 ms |
| plana 100 000 linhas (1.8 MB) | 100 000 | 100 000 | 13.6 ms | 1.76 ms | 3.22 ms |

Desde 19/11/2026 (#125), a linha de 10 milhões passa do teto de 5 000 000 eventos e é recusada; a maior carga aceita passou a ser a `aninhada 5000 × 1000` (ver 6.3).

O frontend cresce com o tamanho do **arquivo**, e o interpretador com o número de **eventos**. Um arquivo de 100 mil linhas passa pelo Bison sem estourar a pilha, porque a lista de comandos do `parser.y` é recursiva à esquerda.

### 4.3 Executável `compilador`

Com `compilador -q --no-audio`, medido com `/usr/bin/time -l`, mediana de três execuções:

| Programa | Eventos | Tempo | Memória máxima (RSS) |
|---|--:|--:|--:|
| `tests/estresse/denso_1500bpm.mus` | 6 000 | < 0.01 s | 6.3 MB |
| `tests/estresse/mista.mus` | 5 000 | < 0.01 s | 6.3 MB |
| `tests/estresse/um_milhao.mus` | 1 000 000 | 0.03 s | 99 MB |
| aninhada 10000 × 1000 | 10 000 000 | 0.38 s | 1.1 GB |
| aninhada 100000 × 1000 | 100 000 000 | 8.7 s | 3.9 a 5.4 GB |

As duas últimas linhas passam do teto de eventos desde 19/11/2026 e hoje são recusadas (ver 6.3).

A memória cresce cerca de **100 bytes por evento**. O `SoundEvent` tem 32 bytes. O resto vem da folga de capacidade do `std::vector` e do buffer temporário da ordenação estável. Quando estas medições foram feitas, o caminho com áudio guardava mais duas cópias: a ordenada do `carregarFila` e a da fila SPSC. Desde 19/11/2026, o `carregarFila` não copia a Timeline que já chega ordenada, e sobra só a da fila (ver 6.3).

**Com a placa de som.** Para conferir o relógio de parede, dois programas de 6 s com `volume 0` tocaram no dispositivo real (três execuções cada):

| Programa | Eventos | Esperado | Medido | Aviso de underrun no terminal |
|---|--:|--:|--:|---|
| 1500 BPM, figura 0.25 | 600 | 6 s | 6.10 a 6.32 s | nenhum |
| 2400 BPM, figura 0.25 | 960 | 6 s | 6.09 a 6.13 s | nenhum |

Os cerca de 0.1 a 0.3 s a mais são a abertura do dispositivo e a inicialização do processo. O falso underrun de 2400 BPM (seção 4.1) não apareceu no terminal: ele é levantado nos últimos milissegundos, e o `waitUntilFinished` só olha a bandeira a cada 50 ms.

## 5. Envelope aceito

Os cenários que o CTest afirma, e que portanto **não podem travar nem perder nota**:

- andamento de **60 a 6000 BPM** nos cenários da grade de estresse;
- nota mais curta de **2,5 ms** (`kMenorNotaAceitaSegundos` em `cargas.h`), validada pela correção da DSP #123;
- até **um milhão de eventos** compilados, ordenados e entregues à fila. Nessa carga não há render: a reprodução é verificada nas cargas menores, cerca de 1 minuto e meio de áudio renderizado e examinado amostra a amostra;
- arquivo de **20 mil linhas**;
- no máximo **5 000 000 eventos** e **10 000 000 voltas de `repita`** por programa, somando os aninhados (#125). Acima disso, o programa é recusado com erro de execução;
- **64 eventos sobrepostos** direto na fila, em ordem de início (oito grupos de oito simultâneos), duas vezes as 32 vozes do motor: a saída fica limitada e todas as vozes voltam ao silêncio.

## 6. Achados

Cada falha encontrada tem issue própria. Nenhuma foi corrigida aqui: a INT-07 mede, e a correção muda o comportamento de outras partes.

### 6.1 Notas abaixo de 10 ms saem atenuadas ou mudas (#123)

O envelope do motor tem ataque e release fixos de 5 ms cada (`AudioEngine::renderRealTime`). O release é descontado da duração da nota, e o ataque não é encurtado:

- **entre 5 e 10 ms**, o ataque não termina antes do release começar. A 2400 BPM (6.25 ms), o pico de **todas** as 1 066 notas fica em 0.25;
- **até 5 ms**, a nota passa direto para o release, a partir do nível zero. A 6000 BPM (2.5 ms), **nenhuma** das 2 666 notas soa, e a saída é silêncio puro.

A mesma conta faz a voz voltar ao repouso antes do fim do evento. Quando o último evento do programa é curto, a fila acaba antes da duração esperada e o motor levanta um **underrun que não existiu**.

**Correção da DSP #123 (22/11/2026).**

O `AudioEngine::renderRealTime` passou a adaptar o ataque e o
release à duração de cada evento. Notas longas preservam os
tempos originais de 5 ms, enquanto notas mais curtas dividem
os frames disponíveis entre ataque e release.

Também foram adicionados testes de regressão para notas de
1 e 2 frames, notas de 2,5 ms e 6,25 ms e detecção de underrun.
Notas sintetizadas de 1 e 2 frames recebem tratamento específico
de fase inicial para evitar silêncio completo.

Medições após a correção, realizadas pelo revisor em
build Release, na mesma máquina para comparar a versão
anterior com a implementação da DSP #123:

| Andamento | Duração da nota | Antes da correção | Após a correção |
|---|---|---|---|
| 2400 BPM | 6,25 ms | 1066 notas atenuadas, pico 0.226, 2 underruns | 0 atenuadas, pico 0.944, 0 underruns |
| 6000 BPM | 2,5 ms | 2666 notas mudas, pico 0.000, 2 underruns | 0 mudas, pico 0.895, 0 underruns |

Nas duas cargas, o fator de geração de áudio após a
correção ficou em aproximadamente 1100 vezes o tempo real,
conforme as medições do revisor.

Os resultados originais da seção 4 permanecem como registro
histórico do comportamento anterior à correção.

As medições em Release foram realizadas na mesma máquina
para permitir a comparação antes e depois. Esses resultados
não devem ser comparados diretamente com os valores
históricos obtidos em outros ambientes.

O limite mínimo aceito pelos testes de estresse foi reduzido
de 10 ms para 2,5 ms.

**Limitação:** notas de 1 e 2 frames não comportam um envelope
com ataque e release suaves completos. O tratamento específico
garante uma amostra sintetizada não nula, mas não comprova
ausência de cliques nesses casos extremos.

### 6.2 `repita` e `oitava` com valor não finito (#124)

O interpretador converte `double` para inteiro (`static_cast<long>` no `repita`, `static_cast<int>` na `oitava`) sem checar a faixa. Com valor fora dela, o comportamento é indefinido em C++:

| Programa | O que acontece |
|---|---|
| `repita 1/0 vezes { toca do4 por 1 }` | **trava**: no ARM o infinito satura em `LONG_MAX` voltas, e a Timeline cresce até esgotar a memória |
| `repita 1000000000000 vezes { }` | **trava**: 10¹² voltas sem fazer nada. Interrompido após 15 s |
| `repita 0/0 vezes { toca do4 por 1 }` | compila sem erro e gera **0 eventos** |
| `oitava 1/0` e depois `toca do por 1` | compila sem erro e toca **8.18 Hz** (MIDI 0) |

**Corrigido em 19/11/2026.** O interpretador checa o valor antes do cast. A contagem do `repita` precisa ser finita, não negativa e caber no teto de voltas, e a `oitava` precisa truncar para −1 a 9. Os quatro programas acima saem com código 1, em menos de 0,2 s, e a mensagem diz qual comando falhou:

| Programa | Erro |
|---|---|
| `repita 1/0 vezes { ... }` | repita precisa de uma quantidade finita, recebeu inf |
| `repita 1000000000000 vezes { }` | repita pede 1000000000000 voltas, acima do limite de 10000000 por programa |
| `repita 0/0 vezes { ... }` | repita precisa de uma quantidade finita, recebeu nan |
| `oitava 1/0` | oitava precisa estar entre -1 e 9, recebeu inf |

Cobertos por `test_interpreter.cpp` e pelos testes de CLI `ir_recusa_repita_infinito` e `ir_recusa_oitava_invalida`.

### 6.3 Sem teto de eventos (#125)

Nada limita o tamanho da Timeline. 10⁸ eventos compilam em cerca de 9 s e ocupam de 4 a 5 GB. Algumas linhas de `.mus` bastam para pedir 10⁹, e aí o processo esgota a memória em vez de recusar o programa com uma mensagem.

**Corrigido em 19/11/2026.** O interpretador tem dois tetos por programa (`Interpretador::kMaximoEventos` e `kMaximoVoltas`):

- **5 000 000 eventos.** O evento que passaria do teto é recusado antes de entrar na Timeline. São cerca de 500 MB, 35 horas de música a 600 BPM e 5 vezes a maior carga aceita acima (10⁶).
- **10 000 000 voltas de `repita`**, somando todos os do programa. Uma volta vazia custa cerca de 5 ns, então o teto custa uns 50 ms. Ele existe porque `repita` aninhados com corpo vazio não geram evento, mas gastam CPU sem fim.

Além disso, `carregarFila` não copia mais a Timeline quando ela já chega ordenada, que é o caso da que sai do interpretador. Isso tira uma cópia de 32 bytes por evento do caminho com áudio.

Medido com `compilador -q --no-audio` (Apple M4, Release, `/usr/bin/time -l`, três execuções):

| Programa | Resultado | CPU | Memória máxima (RSS) |
|---|---|--:|--:|
| programa vazio | código 0 | 0.15 s | 249 MB |
| `repita 100000 vezes { repita 10000 vezes { toca do4 por 1 } }` (10⁹ pedidos) | código 1: passa do limite de 5000000 eventos | 0.6 s | 785 MB |

A base de 249 MB é o banco de samples da INT-05 (#31), carregado em toda execução desde 04/11. As medições da seção 4.3 são de antes dele, por isso partem de 6.3 MB. Cobertos por `test_interpreter.cpp`, pelo caso de 6 milhões de eventos em `test_estresse.cpp` e pelo teste de CLI `estresse_cli_recusa_acima_do_teto`.

## 7. Limitações deste teste

- **Uma máquina só.** Os tempos são de um Apple M4 em Release. O CI roda as afirmações em Linux e macOS, em Debug e sob sanitizer, mas não mede tempo.
- **Sobreposição só na fila.** A linguagem não tem construção de notas simultâneas, então a sobreposição é testada empurrando eventos direto na fila, abaixo do compilador.
- **A reprodução é examinada fora do dispositivo.** O render chama o mesmo código que o callback, mas não passa pelo agendamento da thread de áudio do sistema operacional. Underrun real, causado pelo SO, fica com a DSP-04 (#24) e a BUG-AUDIO (#109).
- **Drift temporal** entre o instante programado e o frame tocado fica com a MAT-08 (#45). Aqui se afirma só que nada soa depois do fim esperado e que o último início de um milhão de eventos bate com a conta fechada.
