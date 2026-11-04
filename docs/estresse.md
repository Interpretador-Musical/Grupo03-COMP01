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
| Algum evento se perde entre o `.mus` e a fila de áudio? | Não. A contagem bate com a fórmula fechada em todas as cargas, até 10 milhões de eventos. |
| O motor de áudio dá conta da densidade? | Sim. Ele gera o áudio cerca de **2 200 vezes** mais rápido que o tempo real. |
| Até onde vai o envelope aceito? | Nota de **10 ms** ou mais: até **1500 BPM** com figura de 0.25 tempo (100 eventos por segundo). |
| O que quebra fora dele? | Nota abaixo de 10 ms sai atenuada, e abaixo de 5 ms sai muda (#123). `repita` e `oitava` com valor não finito travam ou erram em silêncio (#124). Não há teto de eventos, e a memória cresce cerca de 100 bytes por evento (#125). |

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
| `tests/estresse/*.mus` | Três cargas para o executável `compilador` de verdade (`estresse_cli_*` no CTest), com a conta esperada no comentário do topo. |
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

O pico de 0.952 é o limitador do master bus trabalhando sobre uma nota de volume 1.0 (DSP-10, #57). Na mista, o volume máximo do programa é 0.8.

### 4.2 Volume (só compilação)

Sem render: um milhão de eventos a 600 BPM são quase 7 horas de áudio.

| Carga | Eventos | Esperado | Frontend | Semântica | Interpretação |
|---|--:|--:|--:|--:|--:|
| aninhada 100 × 1000 | 100 000 | 100 000 | 0.01 ms | 0.00 ms | 2.3 ms |
| aninhada 1000 × 1000 | 1 000 000 | 1 000 000 | 0.00 ms | 0.00 ms | 24.5 ms |
| aninhada 10000 × 1000 | 10 000 000 | 10 000 000 | 0.01 ms | 0.00 ms | 454.5 ms |
| plana 10 000 linhas | 10 000 | 10 000 | 1.65 ms | 0.05 ms | 0.23 ms |
| plana 100 000 linhas (1.8 MB) | 100 000 | 100 000 | 13.6 ms | 1.76 ms | 3.22 ms |

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

A memória cresce cerca de **100 bytes por evento**. O `SoundEvent` tem 32 bytes. O resto vem da folga de capacidade do `std::vector` e do buffer temporário da ordenação estável. No caminho com áudio, `carregarFila` ainda faz uma cópia ordenada e a fila SPSC guarda outra. Ver #125.

**Com a placa de som.** Para conferir o relógio de parede, dois programas de 6 s com `volume 0` tocaram no dispositivo real (três execuções cada):

| Programa | Eventos | Esperado | Medido | Aviso de underrun no terminal |
|---|--:|--:|--:|---|
| 1500 BPM, figura 0.25 | 600 | 6 s | 6.10 a 6.32 s | nenhum |
| 2400 BPM, figura 0.25 | 960 | 6 s | 6.09 a 6.13 s | nenhum |

Os cerca de 0.1 a 0.3 s a mais são a abertura do dispositivo e a inicialização do processo. O falso underrun de 2400 BPM (seção 4.1) não apareceu no terminal: ele é levantado nos últimos milissegundos, e o `waitUntilFinished` só olha a bandeira a cada 50 ms.

## 5. Envelope aceito

Os cenários que o CTest afirma, e que portanto **não podem travar nem perder nota**:

- andamento de **60 a 1500 BPM**;
- nota mais curta de **10 ms** (`kMenorNotaAceitaSegundos` em `cargas.h`);
- até **um milhão de eventos** compilados, ordenados e entregues à fila. Nessa carga não há render: a reprodução é verificada nas cargas menores, cerca de 1 minuto e meio de áudio renderizado e examinado amostra a amostra;
- arquivo de **20 mil linhas**;
- **64 eventos sobrepostos** direto na fila, em ordem de início (oito grupos de oito simultâneos), duas vezes as 32 vozes do motor: a saída fica limitada e todas as vozes voltam ao silêncio.

## 6. Achados

Cada falha encontrada tem issue própria. Nenhuma foi corrigida aqui: a INT-07 mede, e a correção muda o comportamento de outras partes.

### 6.1 Notas abaixo de 10 ms saem atenuadas ou mudas (#123)

O envelope do motor tem ataque e release fixos de 5 ms cada (`AudioEngine::renderRealTime`). O release é descontado da duração da nota, e o ataque não é encurtado:

- **entre 5 e 10 ms**, o ataque não termina antes do release começar. A 2400 BPM (6.25 ms), o pico de **todas** as 1 066 notas fica em 0.25;
- **até 5 ms**, a nota passa direto para o release, a partir do nível zero. A 6000 BPM (2.5 ms), **nenhuma** das 2 666 notas soa, e a saída é silêncio puro.

A mesma conta faz a voz voltar ao repouso antes do fim do evento. Quando o último evento do programa é curto, a fila acaba antes da duração esperada e o motor levanta um **underrun que não existiu**.

### 6.2 `repita` e `oitava` com valor não finito (#124)

O interpretador converte `double` para inteiro (`static_cast<long>` no `repita`, `static_cast<int>` na `oitava`) sem checar a faixa. Com valor fora dela, o comportamento é indefinido em C++:

| Programa | O que acontece |
|---|---|
| `repita 1/0 vezes { toca do4 por 1 }` | **trava**: no ARM o infinito satura em `LONG_MAX` voltas, e a Timeline cresce até esgotar a memória |
| `repita 1000000000000 vezes { }` | **trava**: 10¹² voltas sem fazer nada. Interrompido após 15 s |
| `repita 0/0 vezes { toca do4 por 1 }` | compila sem erro e gera **0 eventos** |
| `oitava 1/0` e depois `toca do por 1` | compila sem erro e toca **8.18 Hz** (MIDI 0) |

### 6.3 Sem teto de eventos (#125)

Nada limita o tamanho da Timeline. 10⁸ eventos compilam em cerca de 9 s e ocupam de 4 a 5 GB. Algumas linhas de `.mus` bastam para pedir 10⁹, e aí o processo esgota a memória em vez de recusar o programa com uma mensagem.

## 7. Limitações deste teste

- **Uma máquina só.** Os tempos são de um Apple M4 em Release. O CI roda as afirmações em Linux e macOS, em Debug e sob sanitizer, mas não mede tempo.
- **Sobreposição só na fila.** A linguagem não tem construção de notas simultâneas, então a sobreposição é testada empurrando eventos direto na fila, abaixo do compilador.
- **A reprodução é examinada fora do dispositivo.** O render chama o mesmo código que o callback, mas não passa pelo agendamento da thread de áudio do sistema operacional. Underrun real, causado pelo SO, fica com a DSP-04 (#24) e a BUG-AUDIO (#109).
- **Drift temporal** entre o instante programado e o frame tocado fica com a MAT-08 (#45). Aqui se afirma só que nada soa depois do fim esperado e que o último início de um milhão de eventos bate com a conta fechada.
