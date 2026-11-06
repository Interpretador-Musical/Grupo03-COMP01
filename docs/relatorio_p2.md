---
title: Relatório e Evidências (P2)
layout: default
parent: Documentação
nav_order: 11
---

# Relatório de Verificação e Evidências — Ponto de Controle 2 (P2)

| | |
|---|---|
| **Projeto** | Compilador Musical (Grupo 03) |
| **Disciplina** | Compiladores 1 — Turma 01 (Prof. Sergio Freitas) — FCTE/UnB |
| **Marco** | Ponto de Controle 2 (P2) — Sprint 04 |
| **Data** | 02 de novembro de 2026 |
| **Revisão** | 05 de novembro de 2026: números de teste e situação das issues atualizados para a `main` no commit `ab31121` (fim da Sprint 04), os mesmos do formulário do P2 |
| **Issue de Rastreabilidade** | [DOC-04 (#26)](https://github.com/Interpretador-Musical/Grupo03-COMP01/issues/26) |

---

## 1. Visão Geral e Objetivo

Este relatório formaliza a entrega do **Ponto de Controle 2 (P2)** do Compilador Musical, consolidando todo o avanço teórico e prático desenvolvido pela equipe desde o P1 (Sprints 1 e 2) até o presente momento (Sprints 3 e 4).

O objetivo principal desta entrega é comprovar a maturidade do pipeline de compilação em lote (*batch*), estruturado estritamente segundo as fases clássicas de engenharia de compiladores descritas no guia da disciplina:

`.mus` → **Flex** (Tokens) → **Bison** (AST) → **Semântica** (AST Validada + Símbolos) → **Interpretador** (Timeline IR) → **Engine** (Áudio PCM)

O compilador opera no modelo de **programa finito** (decisão arquitetural de 16/09 registrada na [ADR 001]({{ site.baseurl }}{% link decisao_modelo_batch.md %})): todo o arquivo `.mus` é completamente analisado, validado e convertido em uma representação intermediária linear determinística antes de qualquer reprodução sonora.

---

## 2. Comparativo de Evolução: P1 versus P2

A tabela abaixo sintetiza o salto de engenharia entre os dois pontos de controle:

| Dimensão | Estado no P1 (Sprint 02) | Estado Atual no P2 (Sprint 04) | Impacto / Ganho |
|---|---|---|---|
| **Modelo de Execução** | Conflito conceitual entre *live coding* infinito (modelo Tidal) e programa finito. | **Compilador Batch Finito** formalizado na [ADR 001]({{ site.baseurl }}{% link decisao_modelo_batch.md %}). | Foco 100% alinhado às etapas formais da disciplina de Compiladores. |
| **Gramática & Teoria** | Esboço gramatical inicial no Bison sem formalização teórica. | [Gramática EBNF]({{ site.baseurl }}{% link gramatica_ebnf.md %}) formalizada, com teste de paridade sintática. | Rigor formal com tabela de precedência e associatividade explícitas. |
| **Árvore Sintática (AST)** | Nós simples em C++ sem política clara de gerenciamento de memória. | [AST Tipada em C++17]({{ site.baseurl }}{% link ast.md %}) com `std::variant`, RAII (`ast::Arvore`) e limite de 256 níveis. | Segurança de tipos via `std::variant` / `std::visit` e proteção contra estouro de pilha pelo limite de 256 níveis de profundidade (CMP-08). |
| **Análise Semântica** | Inexistente (a AST ia direto para execução de áudio). | **Analisador Semântico com Tabela de Símbolos**, verificação de escopos estáticos e checagem de tipos. | Rejeição precoce de variáveis não declaradas ou tipos incompatíveis antes de qualquer som. |
| **Representação Intermediária (IR)** | Não formalizada (lista ad-hoc de eventos). | [Timeline de SoundEvent]({{ site.baseurl }}{% link representacao_intermediaria.md %}) formalizada com 6 invariantes e `--emit-timeline`. | IR musical de eventos temporizados (não é código de três endereços); determinismo verificado byte a byte contra CSVs canônicos. |
| **Geração de Áudio** | Síntese direta sujeita a *clipping* e *underruns*. | **Pipeline Lock-Free (SPSC)** com limitador suave (*soft-clipper*) no master bus e mitigação de *underrun*. | Síntese em memória testável sem exigir hardware; tratamento com envelopes anti-clique (attack/release de 5 ms) e limitador no master bus (investigação de chiados residuais em aberto na issue BUG-AUDIO #109). |
| **Qualidade & Testes** | 194 casos de teste, sem análise semântica nem IR para testar. | **409 casos doctest** (99.794 asserções), **22 testes no CTest** com a suíte ponta a ponta (`INT-06`) e o estresse com 1 milhão de eventos (`INT-07`). ASan e TSan rodam em todo PR, sem erros de memória nem corridas detectados nos testes. | Regressões impedidas em nível de compilação, interpretação e execução de linha de comando. |

### Pendências Explícitas e Escopo Futuro
* **Saída-Alvo Musical Definitiva (COMP-C #108):** Atualmente o compilador reproduz os eventos de áudio via engine e exporta a Timeline em CSV auditável (`--emit-timeline`). A geração da saída final como artefato persistente (ex.: arquivo WAV) está planejada como entrega principal da Sprint 05.
* **Limites do compilador (#124, #125) e notas muito curtas (#123):** defeitos encontrados pelo teste de estresse (INT-07, PR #126): `repita` e `oitava` com valor não finito, ausência de teto de eventos e notas abaixo de 10 ms. Planejados para a Sprint 05.
* **Condicionais (`se/entao/senao`) e Funções (`defina/retorna`):** As palavras-chave estão reservadas no léxico, mas suas produções sintáticas foram deliberadamente delimitadas para as sprints posteriores (Sprints 5 e 6). Elas não compõem o escopo avaliativo do P2 para preservar a solidez e a estabilidade da DSL batch atual.
* **Chiados Residuais (BUG-AUDIO #109):** Síntese básica e mixagem polifônica com voice stealing seguro (DSP-05, DSP-07) implementadas; diagnóstico aprofundado de chiados pontuais na reprodução em tempo real alocado para a Sprint 05.

---

## 3. Matriz de Rastreabilidade (Requisitos → Código → Testes → Resultado)

A matriz a seguir detalha como cada estágio do compilador atende aos requisitos do projeto de compiladores da disciplina, indicando claramente os itens implementados e os pendentes para as próximas sprints:

| Estágio / Requisito | Código / Implementação | Suíte de Testes Automatizados | Status / Resultado Comprovado |
|---|---|---|---|
| **1. Análise Léxica**<br>Reconhecimento de notas (`do`–`si`, acidentes `#`/`b`, oitavas), comandos (`toca`, `pausa`, `andamento`, `oitava`, `volume`), repetição (`repita`), números, identificadores e comentários (`//` e `/* */`). | `lexer/lexer.l`<br>`src/frontend/scanner.h`<br>`src/frontend/frontend.cpp` | `tests/test_lexer.cpp`<br>`tests/test_gramatica.cpp`<br>Flag `--tokens` | **Implementado**<br>Tokens com rastreamento estrito de linha e coluna via `YY_USER_ACTION`. Caracteres inválidos reportados sem travamento. |
| **2. Análise Sintática**<br>Gramática livre de contexto LALR(1) sem conflitos shift/reduce, separador obrigatório `por`, precedência de operadores, trilhas paralelas `{ , }` (CMP-07) e proteção contra recursão profunda. | `parser/parser.y`<br>`src/frontend/frontend.cpp` | `tests/test_parser.cpp`<br>`tests/test_gramatica.cpp`<br>`tests/test_trilhas.cpp`<br>`tests/test_profundidade.cpp`<br>`integracao_cli_recusa_erro_sintaxe` | **Implementado**<br>Construção direta da AST com liberação via `%destructor`. Rejeição determinística de sintaxe malformada com indicação de linha e coluna. Limite de 256 níveis de profundidade (`CMP-08`). A EBNF é comparada ao Bison em todas as cadeias de até 5 tokens. |
| **3. Árvore Sintática (AST)**<br>Representação estruturada em memória, separação estrita de comandos e expressões, nós tipados. | `src/interpreter/ast.h`<br>`src/interpreter/ast.cpp` | `tests/test_ast.cpp`<br>`tests/test_memoria.cpp`<br>(Validação ASAN) | **Implementado**<br>Estrutura estática moderna em C++17 com `std::variant`. Gerenciamento RAII completo (`ast::Arvore`) garantindo ausência de vazamentos de memória no ciclo de vida dos nós. |
| **4. Análise Semântica**<br>Construção da Tabela de Símbolos, resolução de escopo estático (blocos `{ }`), tipagem estática (`numero`, `nota`, `booleano`) e checagem de tipos em expressões e comandos. | `src/semantic/tabela_simbolos.h/cpp`<br>`src/semantic/analisador_semantico.h/cpp` | `tests/test_tabela_simbolos.cpp`<br>`tests/test_analisador_semantico.cpp`<br>`integracao_cli_recusa_erro_semantico` | **Implementado**<br>Rejeição precoce de variáveis não declaradas, redeclarações com tipos incompatíveis e operações inválidas. Flag `--simbolos` expõe o mapa de identificadores e endereços. |
| **5. Representação Intermediária (IR)**<br>Geração da `Timeline` contendo quádruplas `(início, duração, frequência, volume)`. Garantia das 6 invariantes formais do IR e exportação CSV. | `src/core/sound_event.h`<br>`src/core/timeline_emit.h/cpp`<br>`src/interpreter/interpreter.h/cpp` | `tests/test_representacao_intermediaria.cpp`<br>`tests/test_timeline_emit.cpp`<br>11 testes CTest (5 `emit_timeline_*`, 6 `ir_*`) | **Implementado**<br>Conversão exata da AST em eventos temporizados: cada trilha avança o seu *playhead*, e um grupo `{ , }` gera eventos simultâneos, ordenados por início. Flag `--emit-timeline` com saída verificada byte a byte contra arquivos canônicos `.csv`. |
| **6. Geração da Saída / Áudio**<br>Desacoplamento entre lógica de compilação e dispositivo de áudio. Fila lock-free SPSC, limitador suave (*soft-clipper*) e prevenção de *underrun*. | `src/core/ring_buffer.h`<br>`src/audio/audio_engine.h/cpp`<br>`src/audio/limiter.h` | `tests/test_ring_buffer.cpp`<br>`tests/test_audio_envelope.cpp`<br>`tests/test_limiter.cpp`<br>`tests/test_audio_underrun.cpp` | **Implementado**<br>Síntese PCM em 44.1 kHz mono com envelopes anti-clique (attack/release de 5 ms). Modo testável em memória sem exigir hardware de som físico. |
| **7. Integração & Falhas Seguras**<br>Execução ponta a ponta do executável `compilador`, validação em modo batch e código de saída apropriado para sucesso (0) e erros (1). | `src/main.cpp`<br>`tests/test_integracao.cpp`<br>`tests/estresse/` | `mus_tests` (INT-06)<br>6 testes de CLI no CTest (`integracao_cli_*`)<br>`mus_tests_estresse` e 3 testes `estresse_cli_*` (INT-07) | **Implementado**<br>Pipeline completo testado em casos válidos e inválidos. Falhas léxicas, sintáticas e semânticas interrompem antes de gerar IR ou som. O estresse compila e enfileira 1 milhão de eventos sem perda. |
| **8. Saída-Alvo Musical Definitiva (COMP-C #108)**<br>Geração e persistência do artefato musical em disco (ex.: exportador de arquivo WAV padronizado). | `src/main.cpp`<br>`src/audio/` | Testes de integridade de arquivo de áudio | **Pendente (Sprint 05)**<br>Atualmente a saída intermediária é auditada via `--emit-timeline` (CSV) e executada via engine de áudio em tempo real. A exportação definitiva em disco será entregue na Sprint 05. |
| **9. Robustez da AST (CMP-04 #21)**<br>Construções aninhadas e gerenciamento de memória dos nós. | `src/interpreter/ast.h/cpp` | `tests/test_ast.cpp`<br>`tests/test_profundidade.cpp`<br>`tests/test_memoria.cpp` | **Concluído (03/11, PR #130)**<br>A CMP-04 virou auditoria e testes de aninhamento combinado: raiz em `ast::Arvore` (`std::unique_ptr`), filhos liberados por `ast::liberar()`, `%destructor` no Bison e limite de 256 níveis (CMP-08 #44). A troca dos ponteiros dos filhos por `std::unique_ptr` não é necessária para o escopo atual. |
| **10. Diagnóstico de Qualidade Sonora (BUG-AUDIO #109)**<br>Investigação e eliminação de chiados residuais e estalos relatados em transições densas. | `src/audio/audio_engine.cpp` | Testes de estresse e inspeção de amostras | **Pendente (Sprint 05)**<br>Limitador master bus (DSP-10) integrado; análise aprofundada de buffer underrun agendada para a próxima sprint. |
| **11. Condicionais e Funções**<br>Construções sintáticas `se/entao/senao` e declaração de sub-rotinas `defina/retorna`. | `lexer/lexer.l`<br>`parser/parser.y` | `tests/test_parser.cpp` | **Pendente (Sprints 05/06)**<br>Tokens reservados no analisador léxico; gramática e interpretação deliberadamente postergadas para manter o escopo enxuto no P2. |

---

## 4. Evidências de Validação Automatizada e Garantia de Qualidade

A integridade do compilador é garantida por testes automatizados executados localmente e nos fluxos de Integração Contínua (GitHub Actions em Ubuntu e macOS). Os resultados abaixo são do commit `ab31121` da `main` (fim da Sprint 04), compilado em Debug como na CI, num macOS (Apple M4). São os mesmos números informados no formulário do P2.

### 4.1 Resultados da Suíte CTest (100% de Sucesso)
Execução da suíte completa de testes de regressão e integração via CTest:

```text
Test project .../build
      Start  1: mus_tests
 1/22 Test  #1: mus_tests ...................................   Passed    3.99 sec
      Start  2: mus_tests_estresse
 2/22 Test  #2: mus_tests_estresse ..........................   Passed    1.17 sec
      Start  3: emit_timeline_escala
 3/22 Test  #3: emit_timeline_escala ........................   Passed    1.92 sec
      Start  4: emit_timeline_arpejo
 4/22 Test  #4: emit_timeline_arpejo ........................   Passed    1.60 sec
      Start  5: emit_timeline_polirritmia
 5/22 Test  #5: emit_timeline_polirritmia ...................   Passed    1.60 sec
      Start  6: emit_timeline_recusa_erro_sintaxe
 6/22 Test  #6: emit_timeline_recusa_erro_sintaxe ...........   Passed    0.01 sec
      Start  7: emit_timeline_recusa_erro_semantico
 7/22 Test  #7: emit_timeline_recusa_erro_semantico .........   Passed    0.01 sec
      Start  8: ir_notas_e_pausas
 8/22 Test  #8: ir_notas_e_pausas ...........................   Passed    1.60 sec
      Start  9: ir_andamento
 9/22 Test  #9: ir_andamento ................................   Passed    1.61 sec
      Start 10: ir_repeticao
10/22 Test #10: ir_repeticao ................................   Passed    1.61 sec
      Start 11: ir_expressoes
11/22 Test #11: ir_expressoes ...............................   Passed    1.63 sec
      Start 12: ir_trilhas
12/22 Test #12: ir_trilhas ..................................   Passed    1.61 sec
      Start 13: ir_recusa_volume_invalido
13/22 Test #13: ir_recusa_volume_invalido ...................   Passed    0.81 sec
      Start 14: integracao_cli_escala_no_audio
14/22 Test #14: integracao_cli_escala_no_audio ..............   Passed    0.80 sec
      Start 15: integracao_cli_arpejo_no_audio
15/22 Test #15: integracao_cli_arpejo_no_audio ..............   Passed    0.80 sec
      Start 16: integracao_cli_polirritmia_no_audio
16/22 Test #16: integracao_cli_polirritmia_no_audio .........   Passed    0.80 sec
      Start 17: integracao_cli_recusa_arquivo_inexistente
17/22 Test #17: integracao_cli_recusa_arquivo_inexistente ...   Passed    0.00 sec
      Start 18: integracao_cli_recusa_erro_sintaxe
18/22 Test #18: integracao_cli_recusa_erro_sintaxe ..........   Passed    0.00 sec
      Start 19: integracao_cli_recusa_erro_semantico
19/22 Test #19: integracao_cli_recusa_erro_semantico ........   Passed    0.00 sec
      Start 20: estresse_cli_denso_1500bpm
20/22 Test #20: estresse_cli_denso_1500bpm ..................   Passed    0.81 sec
      Start 21: estresse_cli_mista
21/22 Test #21: estresse_cli_mista ..........................   Passed    0.81 sec
      Start 22: estresse_cli_um_milhao_no_audio
22/22 Test #22: estresse_cli_um_milhao_no_audio .............   Passed    1.47 sec

100% tests passed, 0 tests failed out of 22

Total Test time (real) =  24.67 sec
```

Os 22 testes cobrem: a suíte doctest (`mus_tests`) e a de estresse (`mus_tests_estresse`); a IR emitida comparada byte a byte com CSVs canônicos (`emit_timeline_*` e `ir_*`, 11 testes, incluindo a polirritmia e as trilhas); o executável ponta a ponta, inclusive a recusa de arquivo inexistente e de erros sintáticos e semânticos (`integracao_cli_*`, 6 testes); e o estresse pela CLI, com 1500 BPM, carga mista e 1 milhão de eventos (`estresse_cli_*`, 3 testes).

### 4.2 Resultados da Suíte Unitária Doctest
A suíte interna (`mus_tests`) cobre todas as unidades de tradução do compilador, em 39 arquivos de teste:

```text
[doctest] doctest version is "2.4.11"
===============================================================================
[doctest] test cases:   409 |   409 passed | 0 failed | 2 skipped
[doctest] assertions: 99794 | 99794 passed | 0 failed |
[doctest] Status: SUCCESS!
```

Os 2 casos pulados são os de perfil da SCH-07 (`tests/test_perfil.cpp`), que só rodam sob o Valgrind, com `--no-skip`.

### 4.3 Sanitizadores Dinâmicos: Memória e Concorrência
* **AddressSanitizer (ASAN) & UndefinedBehaviorSanitizer (UBSan):** Compilado com `-fsanitize=address,undefined -g -O1`. A suíte completa e os programas de exemplo executam sem acessos ilegais à memória nem vazamentos detectados.
* **ThreadSanitizer (TSAN):** Compilado com `-fsanitize=thread -g -O1` sob `TSAN_OPTIONS=halt_on_error=1`. Nenhuma condição de corrida (*data race*) detectada nos cenários concorrentes que a suíte exercita, entre o processamento e a fila de áudio lock-free.
* **Evidência Automatizada na CI (GitHub Actions):** Ambas as suítes instrumentadas rodam em cada pull request do projeto. No commit `ab31121`, os quatro jobs (Ubuntu, macOS, ASan e TSan) passaram: [Execução da CI no GitHub Actions #42152104046](https://github.com/Interpretador-Musical/Grupo03-COMP01/actions/runs/42152104046).

---

## 5. Demonstração Prática com Exemplos Canônicos

O compilador inclui programas `.mus` oficiais na pasta `exemplos/` demonstrando a riqueza da linguagem e a geração de seus artefatos:

### 5.1 Exemplo: `exemplos/arpejo.mus`
Programa com 4 repetições de um acorde arpejado, seguido de nota com transposição calculada (`la4 - 2`) e pausa final:

```text
andamento 120
volume 0.8
tempo = 0.5

repita 4 vezes {
    toca do4  por tempo
    toca mi4  por tempo
    toca sol4 por tempo
}

toca la4 - 2 por tempo * 2
pausa por 1.0
```

#### A. Tabela de Símbolos Gerada (`--simbolos`):
Demonstra a detecção de identificador, tipo inferido, escopo global (nível 0) e linhas de uso:

```text
$ ./build/compilador --simbolos exemplos/arpejo.mus
  endereço  nome             tipo      escopo  declarada  usos (linhas)
  --------  ---------------  --------  ------  ---------  -------------
         0  tempo            numero         0     9:1     12, 13, 14, 19

1 símbolos
```

#### B. Representação Intermediária Emitida (`--emit-timeline`):
Timeline com 14 quádruplas sequenciais determinísticas (12 notas do loop + 1 fechamento transposto + 1 pausa):

```text
$ ./build/compilador -q --emit-timeline exemplos/arpejo.mus
indice,inicio_s,duracao_s,frequencia_hz,volume
1,0.000000,0.250000,261.625565,0.800000
2,0.250000,0.250000,329.627557,0.800000
3,0.500000,0.250000,391.995436,0.800000
4,0.750000,0.250000,261.625565,0.800000
5,1.000000,0.250000,329.627557,0.800000
6,1.250000,0.250000,391.995436,0.800000
7,1.500000,0.250000,261.625565,0.800000
8,1.750000,0.250000,329.627557,0.800000
9,2.000000,0.250000,391.995436,0.800000
10,2.250000,0.250000,261.625565,0.800000
11,2.500000,0.250000,329.627557,0.800000
12,2.750000,0.250000,391.995436,0.800000
13,3.000000,0.500000,391.995436,0.800000
14,3.500000,0.500000,0.000000,0.000000
```

### 5.2 Exemplo: `exemplos/escala.mus`
Demonstra notas da escala de Dó maior com oitava nominal, seguidas de notas acidentadas com sustenido (`fa#4`) e bemol (`sib3`):

```text
$ ./build/compilador -q --emit-timeline exemplos/escala.mus
indice,inicio_s,duracao_s,frequencia_hz,volume
1,0.000000,0.600000,261.625565,1.000000
2,0.600000,0.600000,293.664768,1.000000
3,1.200000,0.600000,329.627557,1.000000
4,1.800000,0.600000,349.228231,1.000000
5,2.400000,0.600000,391.995436,1.000000
6,3.000000,0.600000,440.000000,1.000000
7,3.600000,0.600000,493.883301,1.000000
8,4.200000,0.600000,523.251131,1.000000
9,4.800000,0.300000,369.994423,1.000000
10,5.100000,0.300000,233.081881,1.000000
```

### 5.3 Exemplo: `exemplos/polirritmia.mus` (trilhas paralelas, CMP-07)
Grupos `{ trilha, trilha }` começam no mesmo instante. O primeiro grupo toca três contra quatro dentro de um tempo, duas vezes; depois vêm um acorde de três trilhas e uma melodia sobre um baixo. A 90 BPM, a IR tem 22 eventos e 6 s de música; os eventos simultâneos aparecem com o mesmo `inicio_s`:

```text
$ ./build/compilador -q --emit-timeline exemplos/polirritmia.mus
indice,inicio_s,duracao_s,frequencia_hz,volume
1,0.000000,0.222222,261.625565,1.000000
2,0.000000,0.166667,195.997718,1.000000
3,0.166667,0.166667,195.997718,1.000000
4,0.222222,0.222222,261.625565,1.000000
5,0.333333,0.166667,195.997718,1.000000
6,0.444444,0.222222,261.625565,1.000000
7,0.500000,0.166667,195.997718,1.000000
8,0.666667,0.222222,261.625565,1.000000
9,0.666667,0.166667,195.997718,1.000000
10,0.833333,0.166667,195.997718,1.000000
11,0.888889,0.222222,261.625565,1.000000
12,1.000000,0.166667,195.997718,1.000000
13,1.111111,0.222222,261.625565,1.000000
14,1.166667,0.166667,195.997718,1.000000
15,1.333333,1.333333,261.625565,1.000000
16,1.333333,1.333333,329.627557,1.000000
17,1.333333,1.333333,391.995436,1.000000
18,2.666667,0.666667,523.251131,1.000000
19,2.666667,2.666667,65.406391,0.500000
20,3.333333,0.666667,587.329536,1.000000
21,4.000000,1.333333,659.255114,1.000000
22,5.333333,0.666667,261.625565,1.000000
```

### 5.4 Exemplo: `exemplos/erros_semanticos.mus` (recusa antes da IR)
O programa é válido para o parser e traz um erro semântico de cada tipo. A análise semântica recusa todos, com linha e coluna, e nenhum evento é gerado (código de saída 1):

```text
$ ./build/compilador exemplos/erros_semanticos.mus
[ERROR] erro semântico: Linha 14, Coluna 14: 'frase' usado antes de ser declarado ou fora de escopo
[ERROR] erro semântico: Linha 17, Coluna 1: redeclaração inválida: 'tempo' é numero, recebeu booleano
[ERROR] erro semântico: Linha 20, Coluna 14: a duração do toca espera numero, recebeu nota
[ERROR] erro semântico: Linha 21, Coluna 6: operação inválida: nota + nota
[ERROR] erro semântico: Linha 24, Coluna 14: 'compasso' usado antes de ser declarado ou fora de escopo
```

---

## 6. Instruções de Reprodução e Execução

Para reproduzir integralmente os passos e resultados descritos neste documento no Linux, no macOS ou no Windows (via WSL):

```bash
# 1. Clonar o repositório
git clone https://github.com/Interpretador-Musical/Grupo03-COMP01.git
cd Grupo03-COMP01

# 2. Configurar e compilar o projeto em modo Debug
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j

# 3. Executar a suíte de testes completa via CTest
ctest --test-dir build --output-on-failure

# 4. Executar os exemplos e verificar tabelas e saídas
./build/compilador --tokens exemplos/arpejo.mus
./build/compilador --simbolos exemplos/arpejo.mus
./build/compilador -q --emit-timeline exemplos/arpejo.mus
./build/compilador -q --emit-timeline exemplos/polirritmia.mus
./build/compilador --simbolos exemplos/erros_semanticos.mus   # tabela + recusa dos erros

# 5. Executar a reprodução musical completa (requer ambiente com áudio habilitado)
./build/compilador exemplos/arpejo.mus
```

---

## 7. Conclusão da Entrega P2

O Compilador Musical atinge o Ponto de Controle 2 com o núcleo do pipeline batch (análise léxica, sintática, AST, tabela de símbolos, análise semântica e representação intermediária formalizada) devidamente implementado, conectado e testado, com geração preliminar de áudio via engine e definição da saída-alvo final planejada para a Sprint 05 (COMP-C #108). A arquitetura em lote garante estabilidade e rigor conceitual, provendo diagnósticos precisos em tempo de compilação e execução musical determinística.
