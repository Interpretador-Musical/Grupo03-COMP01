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
| **Data** | 02 de Novembro de 2026 |
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
| **Representação Intermediária (IR)** | Não formalizada (lista ad-hoc de eventos). | [Timeline de SoundEvent]({{ site.baseurl }}{% link representacao_intermediaria.md %}) formalizada com 6 invariantes e `--emit-timeline`. | Equivalente a código de três endereços (3AC) pós-otimização; determinismo verificado byte a byte. |
| **Geração de Áudio** | Síntese direta sujeita a *clipping* e *underruns*. | **Pipeline Lock-Free (SPSC)** com limitador suave (*soft-clipper*) no master bus e mitigação de *underrun*. | Síntese em memória testável sem exigir hardware; tratamento com envelopes anti-clique (attack/release de 5 ms) e limitador no master bus (investigação de chiados residuais em aberto na issue BUG-AUDIO #109). |
| **Qualidade & Testes** | Testes unitários dispersos. | **Suíte de Integração Ponta a Ponta** (`INT-06`), 15 testes no CTest, ASAN (zero vazamentos) e TSAN (zero corridas). | Regressões impedidas em nível de compilação, interpretação e execução de linha de comando. |

### Pendências Explícitas e Escopo Futuro
* **Saída-Alvo Musical Definitiva (COMP-C #108):** Atualmente o compilador reproduz os eventos de áudio via engine e exporta a Timeline em CSV auditável (`--emit-timeline`). A geração da saída final como artefato persistente (ex.: arquivo WAV) está planejada como entrega principal da Sprint 05.
* **Refatoração da AST com Smart Pointers (CMP-04 #21):** A AST atual possui robustez de tipos e proteção contra recursão profunda (CMP-08 #44); a migração completa para nós alocados via `std::unique_ptr` segue em andamento para acomodar nós aninhados mais complexos.
* **Condicionais (`se/entao/senao`) e Funções (`defina/retorna`):** As palavras-chave estão reservadas no léxico, mas suas produções sintáticas foram deliberadamente delimitadas para as sprints posteriores (Sprints 5 e 6). Elas não compõem o escopo avaliativo do P2 para preservar a solidez e a estabilidade da DSL batch atual.
* **Chiados Residuais (BUG-AUDIO #109):** Síntese básica e mixagem polifônica com voice stealing seguro (DSP-05, DSP-07) implementadas; diagnóstico aprofundado de chiados pontuais na reprodução em tempo real alocado para a Sprint 05.

---

## 3. Matriz de Rastreabilidade (Requisitos → Código → Testes → Resultado)

A matriz a seguir detalha como cada estágio do compilador atende aos requisitos do projeto de compiladores da disciplina, indicando claramente os itens implementados, em andamento e pendentes para as próximas sprints:

| Estágio / Requisito | Código / Implementação | Suíte de Testes Automatizados | Status / Resultado Comprovado |
|---|---|---|---|
| **1. Análise Léxica**<br>Reconhecimento de notas (`do`–`si`, acidentes `#`/`b`, oitavas), comandos (`toca`, `pausa`, `andamento`, `oitava`, `volume`), repetição (`repita`), números, identificadores e comentários (`//` e `/* */`). | `lexer/lexer.l`<br>`src/frontend/scanner.h`<br>`src/frontend/frontend.cpp` | `tests/test_lexer.cpp`<br>`tests/test_gramatica.cpp`<br>Flag `--tokens` | **Implementado**<br>Tokens com rastreamento estrito de linha e coluna via `YY_USER_ACTION`. Caracteres inválidos reportados sem travamento. |
| **2. Análise Sintática**<br>Gramática livre de contexto LALR(1) sem conflitos shift/reduce, separador obrigatório `por`, precedência de operadores e proteção contra recursão profunda. | `parser/parser.y`<br>`src/frontend/frontend.cpp` | `tests/test_parser.cpp`<br>`tests/test_profundidade.cpp`<br>`integracao_cli_recusa_erro_sintaxe` | **Implementado**<br>Construção direta da AST com liberação via `%destructor`. Rejeição determinística de sintaxe malformada com indicação de linha e coluna. Limite de 256 níveis de profundidade (`CMP-08`). |
| **3. Árvore Sintática (AST)**<br>Representação estruturada em memória, separação estrita de comandos e expressões, nós tipados. | `src/interpreter/ast.h`<br>`src/interpreter/ast.cpp` | `tests/test_ast.cpp`<br>`tests/test_memoria.cpp`<br>(Validação ASAN) | **Implementado**<br>Estrutura estática moderna em C++17 com `std::variant`. Gerenciamento RAII completo (`ast::Arvore`) garantindo ausência de vazamentos de memória no ciclo de vida dos nós. |
| **4. Análise Semântica**<br>Construção da Tabela de Símbolos, resolução de escopo estático (blocos `{ }`), tipagem estática (`numero`, `nota`, `booleano`) e checagem de tipos em expressões e comandos. | `src/semantic/tabela_simbolos.h/cpp`<br>`src/semantic/analisador_semantico.h/cpp` | `tests/test_tabela_simbolos.cpp`<br>`tests/test_analisador_semantico.cpp`<br>`integracao_cli_recusa_erro_semantico` | **Implementado**<br>Rejeição precoce de variáveis não declaradas, redeclarações com tipos incompatíveis e operações inválidas. Flag `--simbolos` expõe o mapa de identificadores e endereços. |
| **5. Representação Intermediária (IR)**<br>Geração da `Timeline` contendo quádruplas `(início, duração, frequência, volume)`. Garantia das 6 invariantes formais do IR e exportação CSV. | `src/core/sound_event.h`<br>`src/core/timeline_emit.h/cpp`<br>`src/interpreter/interpreter.h/cpp` | `tests/test_representacao_intermediaria.cpp`<br>`tests/test_timeline_emit.cpp`<br>9 testes CTest (4 `emit_timeline_*`, 5 `ir_*`) | **Implementado**<br>Conversão exata da AST em eventos temporais lineares com avanço contíguo do *playhead*. Flag `--emit-timeline` com saída verificada byte a byte contra arquivos canônicos `.csv`. |
| **6. Geração da Saída / Áudio**<br>Desacoplamento entre lógica de compilação e dispositivo de áudio. Fila lock-free SPSC, limitador suave (*soft-clipper*) e prevenção de *underrun*. | `src/core/ring_buffer.h`<br>`src/audio/audio_engine.h/cpp`<br>`src/audio/limiter.h` | `tests/test_ring_buffer.cpp`<br>`tests/test_audio_envelope.cpp`<br>`tests/test_limiter.cpp`<br>`tests/test_audio_underrun.cpp` | **Implementado**<br>Síntese PCM em 44.1 kHz mono com envelopes anti-clique (attack/release de 5 ms). Modo testável em memória sem exigir hardware de som físico. |
| **7. Integração & Falhas Seguras**<br>Execução ponta a ponta do executável `compilador`, validação em modo batch e código de saída apropriado para sucesso (0) e erros (1). | `src/main.cpp`<br>`tests/test_integracao.cpp` | `mus_tests` (INT-06)<br>5 testes de CLI no CTest (`integracao_cli_*`) | **Implementado**<br>Pipeline completo testado em casos válidos e inválidos. Falhas léxicas, sintáticas e semânticas interrompem antes de gerar IR ou som. |
| **8. Saída-Alvo Musical Definitiva (COMP-C #108)**<br>Geração e persistência do artefato musical em disco (ex.: exportador de arquivo WAV padronizado). | `src/main.cpp`<br>`src/audio/` | Testes de integridade de arquivo de áudio | **Pendente (Sprint 05)**<br>Atualmente a saída intermediária é auditada via `--emit-timeline` (CSV) e executada via engine de áudio em tempo real. A exportação definitiva em disco será entregue na Sprint 05. |
| **9. Refatoração de Memória da AST (CMP-04 #21)**<br>Estrutura de nós heterogêneos gerenciada por smart pointers (`std::unique_ptr`). | `src/interpreter/ast.h/cpp` | `tests/test_ast.cpp`<br>`tests/test_profundidade.cpp` | **Em andamento**<br>Proteção contra recursão profunda (CMP-08 #44) e liberação com RAII já funcionais; generalização para smart pointers em desenvolvimento. |
| **10. Diagnóstico de Qualidade Sonora (BUG-AUDIO #109)**<br>Investigação e eliminação de chiados residuais e estalos relatados em transições densas. | `src/audio/audio_engine.cpp` | Testes de estresse e inspeção de amostras | **Pendente (Sprint 05)**<br>Limitador master bus (DSP-10) integrado; análise aprofundada de buffer underrun agendada para a próxima sprint. |
| **11. Condicionais e Funções**<br>Construções sintáticas `se/entao/senao` e declaração de sub-rotinas `defina/retorna`. | `lexer/lexer.l`<br>`parser/parser.y` | `tests/test_parser.cpp` | **Pendente (Sprints 05/06)**<br>Tokens reservados no analisador léxico; gramática e interpretação deliberadamente postergadas para manter o escopo enxuto no P2. |

---

## 4. Evidências de Validação Automatizada e Garantia de Qualidade

A integridade do compilador é garantida por testes automatizados executados tanto localmente (Linux/WSL) quanto nos fluxos de Integração Contínua (GitHub Actions em Ubuntu e macOS):

### 4.1 Resultados da Suíte CTest (100% de Sucesso)
Execução da suíte completa de testes de regressão e integração via CTest:

```text
Test project .../build
      Start  1: mus_tests
 1/15 Test  #1: mus_tests ...................................   Passed    0.66 sec
      Start  2: emit_timeline_escala
 2/15 Test  #2: emit_timeline_escala ........................   Passed    0.07 sec
      Start  3: emit_timeline_arpejo
 3/15 Test  #3: emit_timeline_arpejo ........................   Passed    0.08 sec
      Start  4: emit_timeline_recusa_erro_sintaxe
 4/15 Test  #4: emit_timeline_recusa_erro_sintaxe ...........   Passed    0.07 sec
      Start  5: emit_timeline_recusa_erro_semantico
 5/15 Test  #5: emit_timeline_recusa_erro_semantico .........   Passed    0.06 sec
      Start  6: ir_notas_e_pausas
 6/15 Test  #6: ir_notas_e_pausas ...........................   Passed    0.08 sec
      Start  7: ir_andamento
 7/15 Test  #7: ir_andamento ................................   Passed    0.10 sec
      Start  8: ir_repeticao
 8/15 Test  #8: ir_repeticao ................................   Passed    0.12 sec
      Start  9: ir_expressoes
 9/15 Test  #9: ir_expressoes ...............................   Passed    0.12 sec
      Start 10: ir_recusa_volume_invalido
10/15 Test #10: ir_recusa_volume_invalido ...................   Passed    0.07 sec
      Start 11: integracao_cli_escala_no_audio
11/15 Test #11: integracao_cli_escala_no_audio ..............   Passed    0.02 sec
      Start 12: integracao_cli_arpejo_no_audio
12/15 Test #12: integracao_cli_arpejo_no_audio ..............   Passed    0.03 sec
      Start 13: integracao_cli_recusa_arquivo_inexistente
13/15 Test #13: integracao_cli_recusa_arquivo_inexistente ...   Passed    0.01 sec
      Start 14: integracao_cli_recusa_erro_sintaxe
14/15 Test #14: integracao_cli_recusa_erro_sintaxe ..........   Passed    0.02 sec
      Start 15: integracao_cli_recusa_erro_semantico
15/15 Test #15: integracao_cli_recusa_erro_semantico ........   Passed    0.02 sec

100% tests passed, 0 tests failed out of 15
Total Test time (real) = 1.95 sec
```

### 4.2 Resultados da Suíte Unitária Doctest
A suíte interna (`mus_tests`) cobre todas as unidades de tradução do compilador:

```text
[doctest] doctest version is "2.4.11"
===============================================================================
[doctest] test cases:   304 |   304 passed | 0 failed | 0 skipped
[doctest] assertions: 29185 | 29185 passed | 0 failed |
[doctest] Status: SUCCESS!
```

### 4.3 Sanitizadores Dinâmicos: Memória e Concorrência
* **AddressSanitizer (ASAN) & UndefinedBehaviorSanitizer (UBSan):** Compilado com `-fsanitize=address,undefined -g -O1`. Todos os 304 casos de teste e os programas de exemplo executam com sucesso, sem detecção de acessos ilegais à memória ou vazamentos.
* **ThreadSanitizer (TSAN):** Compilado com `-fsanitize=thread -g -O1` sob `TSAN_OPTIONS=halt_on_error=1`. Executado com sucesso, sem detecção de condições de corrida (*data races*) entre o processamento e a fila de áudio lock-free.
* **Evidência Automatizada na CI (GitHub Actions):** Ambas as suítes instrumentadas rodam em cada pull request do projeto. Os jobs de validação passaram com sucesso total em todos os sanitizadores: [Execução da CI no GitHub Actions #41474041581](https://github.com/Interpretador-Musical/Grupo03-COMP01/actions/runs/41474041581).

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

---

## 6. Instruções de Reprodução e Execução

Para reproduzir integralmente os passos e resultados descritos neste documento no ambiente Linux ou Windows (via WSL):

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

# 5. Executar a reprodução musical completa (requer ambiente com áudio habilitado)
./build/compilador exemplos/arpejo.mus
```

---

## 7. Conclusão da Entrega P2

O Compilador Musical atinge o Ponto de Controle 2 com o núcleo do pipeline batch (análise léxica, sintática, AST, tabela de símbolos, análise semântica e representação intermediária formalizada) devidamente implementado, conectado e testado, com geração preliminar de áudio via engine e definição da saída-alvo final planejada para a Sprint 05 (COMP-C #108). A arquitetura em lote garante estabilidade e rigor conceitual, provendo diagnósticos precisos em tempo de compilação e execução musical determinística.
