# 🎵 Compilador Musical — Grupo 03

![C++17](https://img.shields.io/badge/C++-17-blue.svg)
![Build](https://img.shields.io/badge/build-CMake-brightgreen)
![Testes](https://img.shields.io/badge/testes-22%20CTest%20%7C%20doctest-success)
![CI](https://img.shields.io/badge/CI-ASAN%20%7C%20TSAN-blueviolet)
![Status](https://img.shields.io/badge/status-P2%20(Sprint%2004)-orange)

Uma **Domain-Specific Language (DSL)** criada para transformar código-fonte em música. O programa escrito nesta linguagem não imprime texto: ele é analisado, validado e sintetiza frequências sonoras. O analisador léxico (Flex), o analisador sintático (Bison), a análise semântica (Tabela de Símbolos) e o gerador de representação intermediária (Timeline) são inteiramente autorais, construídos para compilação em lote (*batch*).

Trabalho prático da disciplina de **Compiladores 1** (Turma 01, Prof. Sérgio Freitas) — FCTE/UnB.

📄 **Site Oficial e Documentação:** [https://interpretador-musical.github.io/Grupo03-COMP01/](https://interpretador-musical.github.io/Grupo03-COMP01/)  
📊 **Relatório de Verificação P2:** [Relatório e Evidências do P2](https://interpretador-musical.github.io/Grupo03-COMP01/relatorio_p2.html)  
📋 **Governança e Processo:** [Governança e Práticas da Equipe](https://interpretador-musical.github.io/Grupo03-COMP01/governanca.html)

---

## 🚀 Guia de Instalação Rápida

O compilador é escrito em **C++17** e depende das ferramentas clássicas de engenharia de compiladores (GNU Flex e GNU Bison). O processamento de sinais digitais (DSP) utiliza a biblioteca `miniaudio` (versionada no repositório em `include/miniaudio.h`).

### 1. Dependências do Sistema

- Compilador C++ com suporte a C++17 (`g++` 9+ ou `clang` 10+)
- **CMake** (versão 3.16 ou superior)
- **GNU Flex** (Analisador Léxico)
- **GNU Bison** (Analisador Sintático)
- Bibliotecas de desenvolvimento de áudio do sistema operacional

**Linux (Ubuntu/Debian) e Windows (via WSL — Recomendado):**
```bash
sudo apt update
sudo apt install build-essential cmake flex bison libasound2-dev libpulse-dev -y
```

**macOS (via Homebrew):**
```bash
brew install cmake flex bison
# Recomenda-se exportar o Bison do brew para sobrepor a versão legada da Apple
export PATH="$(brew --prefix bison)/bin:$PATH"
```

### 2. Clonando e Compilando

Clone o repositório e faça a compilação via CMake:

```bash
git clone https://github.com/Interpretador-Musical/Grupo03-COMP01.git
cd Grupo03-COMP01

# Configura o build out-of-source e compila
cmake -S . -B build
cmake --build build -j
```

---

## 💻 Como Usar

O binário executável `compilador` é gerado na pasta `build/`.

```bash
# Ajuda e informações gerais
./build/compilador --help                   # Exibe comandos e opções disponíveis
./build/compilador --version                # Exibe a versão do compilador

# Validação e depuração de hardware de áudio
./build/compilador --test-tone              # Toca senoide de teste de 440 Hz por 3s
./build/compilador --demo                   # Imprime uma timeline de demonstração
./build/compilador --loop                   # Executa o motor de tempo contínuo (Ctrl+C encerra)

# Etapas formais de compilação (inspeção e evidências)
./build/compilador --tokens programa.mus    # [Fase 1] Imprime tabela de tokens (Flex)
./build/compilador --simbolos programa.mus  # [Fase 3] Imprime a Tabela de Símbolos e tipos (Semântica)
./build/compilador --emit-timeline programa.mus  # [Fase 4] Compila e emite a Timeline IR em CSV (sem áudio)

# Execução e reprodução sonora
./build/compilador --no-audio programa.mus  # Compila e valida integralmente sem tocar som
./build/compilador programa.mus             # Pipeline completo: compila, valida e toca o arquivo
```

---

## ⚙️ Arquitetura e Pipeline do Compilador

O projeto opera estritamente sob o **modelo batch de programa finito** ([ADR 001](https://interpretador-musical.github.io/Grupo03-COMP01/decisao_modelo_batch.html)). O arquivo `.mus` de entrada é completamente analisado, validado e traduzido para uma **Representação Intermediária (Timeline)** antes de qualquer emissão sonora.

```text
Código-fonte (.mus)
       │
       ▼
 1. Flex (Lexer)             → Varredura léxica e extração de tokens
       │
       ▼
 2. Bison (Parser)           → Validação sintática LALR(1) e montagem da AST tipada
       │
       ▼
 3. Análise Semântica        → Tabela de Símbolos, escopos estáticos e checagem de tipos
       │
       ▼
 4. Interpretador (IR)       → Geração da Timeline linear de SoundEvents (invariantes formais)
       │
       ▼
 5. Motor de Áudio (DSP)     → Fila lock-free SPSC + miniaudio + Limitador Master → Som PCM
```

1. **Análise Léxica (`Flex`):** Rastreia linha e coluna de cada token (`YY_USER_ACTION`) e rejeita caracteres ilegais.
2. **Análise Sintática (`Bison`):** Gramática livre de contexto sem conflitos shift/reduce, com nós RAII (`ast::Arvore`), `%destructor` para liberação segura e limite de 256 níveis de profundidade (CMP-08).
3. **Análise Semântica:** Módulo autoral que infere tipos na declaração, resolve escopos locais de repetição e trilhas, impede redeclarações inválidas e detecta variáveis não declaradas.
4. **Representação Intermediária (IR):** Timeline ordenada de quádruplas `(início, duração, frequência, volume)` governada por 6 invariantes formais, exportável em CSV determinístico (`--emit-timeline`).
5. **Motor de Áudio:** Ring buffer circular lock-free SPSC com envelopes de 5 ms e limitador suave (*soft-clipper*) no master bus (DSP-10).

---

## 🧪 Testes Automatizados e Qualidade

O projeto conta com rigorosa suíte de testes unitários e de integração executados em cada commit via GitHub Actions (Ubuntu e macOS):

```bash
# Executa todos os testes pelo CTest (15 jobs automatizados)
ctest --test-dir build --output-on-failure

# Ou executa diretamente o binário da suíte doctest
./build/tests/mus_tests
```

### Auditoria com Sanitizers

Para garantir ausência de vazamentos de memória e integridade concorrente:

```bash
# Build com AddressSanitizer (ASAN - detecção de memory leaks e memory corruption)
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug -DENABLE_ASAN=ON
cmake --build build-asan -j
ctest --test-dir build-asan --output-on-failure

# Build com ThreadSanitizer (TSAN - detecção de data races e deadlocks)
cmake -S . -B build-tsan -DCMAKE_BUILD_TYPE=Debug -DENABLE_TSAN=ON
cmake --build build-tsan -j
ctest --test-dir build-tsan --output-on-failure
```

---

## 🎹 A Linguagem (Sintaxe e Capacidades)

A sintaxe adota vocabulário em português estruturado para eliminar ambiguidades:

- **Notas:** Padrão latino (`do`, `re`, `mi`, `fa`, `sol`, `la`, `si`), sustenidos (`#`) e bemóis (`b`).
- **Oitavas:** Anexadas à nota (ex: `do4`, `la4`) ou configuradas globalmente (`oitava 4`).
- **Separador Obrigatório `por`:** Isola a altura da duração, permitindo expressões aritméticas sem ambiguidades (ex.: `toca la4 - 2 por tempo * 2`).
- **Trilhas Paralelas (`{ , }`):** Permitem vozes simultâneas, acordes e polirritmias ([ADR 002](https://interpretador-musical.github.io/Grupo03-COMP01/adr_002_trilhas_paralelas.html)).
- **Controle de Estado:** `andamento <BPM>`, `oitava <n>`, `volume <vol>`.

Exemplo de programa válido (`exemplos/arpejo.mus`):

```text
// Configuração do ambiente global
andamento 120
tempo = 0.5

// Bloco de repetição com escopo lexico local
repita 4 vezes {
    toca do4  por tempo
    toca mi4  por tempo
    toca sol4 por tempo
}

// Trilhas paralelas (acordes e polirritmia simultânea)
{
    toca do4 por 1.0,
    toca mi4 por 1.0,
    toca sol4 por 1.0
}

// Transposição de notas e durações com expressões aritméticas
toca la4 - 2 por tempo * 2
pausa por 1.0
```

---

## 📂 Estrutura do Repositório

```text
├── .github/workflows/    # Workflows de CI (build, CTest, ASAN e TSAN)
├── src/                  # Código-fonte principal em C++17
│   ├── main.cpp          # Entrypoint da CLI e controle das fases de compilação
│   ├── audio/            # Backend de áudio miniaudio, buffer WAV e limitador master
│   ├── cli/              # Parsing de argumentos da linha de comando e logger
│   ├── core/             # Estruturas de dados (SoundEvent, Playhead, Timeline, RingBuffer)
│   ├── engine/           # Loop de tempo monotônico e scheduler
│   ├── frontend/         # Invólucro do Scanner Flex e Parser Bison
│   ├── interpreter/      # Interpretador da AST e emissão da Timeline IR
│   └── semantic/         # Analisador semântico, tabela de símbolos e checagem de tipos
├── lexer/                # Analisador léxico Flex (lexer.l)
├── parser/               # Analisador sintático Bison (parser.y)
├── include/              # Dependências single-header (miniaudio.h, doctest.h)
├── tests/                # Suíte completa de testes unitários e de integração
├── samples/              # Amostras de áudio para testes
├── exemplos/             # Scripts musicais (.mus) de demonstração
└── docs/                 # Site do projeto (GitHub Pages), especificações, ADRs e atas
```

---

## 👥 Integrantes e Rotatividade

O projeto utiliza práticas ágeis com rotatividade de papéis a cada Sprint:

- **Arthur Luiz** ([@ArthurLuizUnB](https://github.com/ArthurLuizUnB))
- **Caio Melo Borges** ([@CaioMelo25](https://github.com/CaioMelo25))
- **Cecília Costa** ([@CeciliaCunha](https://github.com/CeciliaCunha))
- **Julia Oliveira** ([@juliapat18](https://github.com/juliapat18))
- **Marcella Anderle** ([@marcellaanderle](https://github.com/marcellaanderle))
