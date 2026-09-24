# 🎵 Compilador Musical — Grupo 03

![C++17](https://img.shields.io/badge/C++-17-blue.svg)
![Build](https://img.shields.io/badge/build-CMake-brightgreen)
![Status](https://img.shields.io/badge/status-Em_desenvolvimento-orange)

Uma **Domain-Specific Language (DSL)** criada para transformar código em música. O programa escrito nesta linguagem não imprime texto, ele sintetiza frequências sonoras. Todo o analisador léxico (Lexer) e sintático (Parser) são autorais, construídos do zero para processamento em lote (*batch*).

Trabalho da disciplina de **Compiladores 1** (Turma 01, Prof. Sérgio) — FCTE/UnB.

📄 **Site Oficial e Documentação:** [https://interpretador-musical.github.io/Grupo03-COMP01/](https://interpretador-musical.github.io/Grupo03-COMP01/)

---

## 🚀 Guia de Instalação Rápida

O projeto é escrito em **C++17** e depende de ferramentas clássicas de engenharia de compiladores (GNU Flex e GNU Bison). O processamento de sinais digitais (DSP) é feito através da biblioteca `miniaudio` (já versionada na pasta `include/`).

### 1. Dependências do Sistema

- Compilador C++ com suporte a C++17 (`build-essential` ou similar)
- **CMake** (3.16 ou superior)
- **GNU Flex** (Analisador Léxico)
- **GNU Bison** (Analisador Sintático)
- Bibliotecas nativas de áudio do sistema operacional

**Linux (Ubuntu/Debian) e Windows (via WSL - Recomendado):**
```bash
sudo apt update
sudo apt install build-essential cmake flex bison libasound2-dev libpulse-dev -y
```
**macOS (via Homebrew):**

```bash
brew install cmake flex bison
```
(Nota macOS: O áudio usa CoreAudio nativamente. Recomendamos forçar o uso do Bison instalado via Homebrew, pois a versão padrão da Apple (2.3) é muito antiga).

### 2. Clonando e Compilando

Clone o repositório e compile a partir da raiz dele:

```bash
git clone https://github.com/Interpretador-Musical/Grupo03-COMP01.git
cd Grupo03-COMP01

# Gera os arquivos de compilação e o executável
cmake -S . -B build
cmake --build build -j
```

### 3. Como Usar

O executável principal é gerado na pasta `build/`.

```bash
./build/compilador --help                   # Exibe os comandos e opções disponíveis
./build/compilador --test-tone              # Toca uma senoide de 440 Hz por 3s (valida a interface de áudio)
./build/compilador --demo                   # Imprime uma timeline de exemplo
./build/compilador --loop                   # Roda o motor de tempo até Ctrl+C
./build/compilador --tokens programa.mus    # Imprime a tabela de tokens do arquivo
./build/compilador --no-audio programa.mus  # Compila e interpreta sem tocar som
./build/compilador programa.mus             # Lê, compila e toca um arquivo musical
```

## ⚙️ Arquitetura e Pipeline

A arquitetura opera sob uma estrita **separação de responsabilidades** entre a lógica da linguagem e a geração de sinal analógico. O sistema não tenta executar as notas linha por linha em tempo real.

```bash
Código-fonte (.mus)
       │
       ▼
 1. Flex (Lexer)      → Varredura léxica e extração de tokens
       │
       ▼
 2. Bison (Parser)    → Validação gramatical e montagem da AST
       │
       ▼
 3. Interpretador     → Percorre a AST e gera lista de `SoundEvents` em memória
       │
       ▼
 4. Gatilho de "Play" → Envia a timeline temporalizada para a `miniaudio`
```

O núcleo do compilador lê o arquivo, processa a gramática e gera uma lista estática de eventos (contendo `startTime`, `duration`, `frequency` e `volume`). O conceito central da interpretação é o playhead: um cursor de tempo que avança conforme as instruções são lidas. O motor de áudio externo (`miniaudio`) só assume o controle no final do processo, o que garante estabilidade de timing durante a execução.

## 🛠️ Stack Tecnológica

| Camada | Escolha |
|---|---|
| Linguagem Base | C++17 |
| Análise léxica | Flex |
| Análise sintática | Bison |
| Motor de Áudio (DSP) | miniaudio (Single-header C) |
| Automação e Build | CMake |
| Plataforma Homologada | Windows (WSL), Linux, macOS |

## 🎹 A Linguagem (Sintaxe Base)

O vocabulário da DSL é estruturado em português, projetado para evitar ambiguidades semânticas.

- **Notas:** Padrão latino (`do`, `re`, `mi`), com suporte a sustenidos (`#`) e bemóis (`b`).
- **Oitavas:** Anexadas diretamente à nota. Exemplo: `do4` (dó central), `la4` (Lá 440 Hz).
- **Tempo:** Medido em batidas e convertido em segundos pelo compilador.
- **Separador de Duração:** A palavra-chave `por` é obrigatória para separar a ação sonora do seu fator de tempo.

A palavra `por` separa a altura da duração. Sem ela, `toca la4 - 2 por tempo` seria
ambíguo: o parser não saberia se `- 2` pertence à altura ou começa a duração. Com o
separador, os dois lados aceitam expressão e a gramática fica sem conflito nenhum.

Exemplo de um arquivo `.mus` válido:
```bash
// Definindo o estado global
andamento 120
tempo = 0.5

/* Um bloco de repetição agrupa eventos e os agenda
sequencialmente na timeline do playhead */
repita 4 vezes {
    toca do4  por tempo
    toca mi4  por tempo
    toca sol4 por tempo
}

// A linguagem suporta expressões matemáticas nas notas e durações
toca la4 - 2 por tempo * 2   
pausa por 1.0
```

## 📂 Estrutura do Repositório

```text
├── .github/              # Workflows de CI/CD (GitHub Actions para build e testes)
├── src/                  # Código-fonte principal C++
│   ├── main.cpp          # Entrypoint da CLI
│   ├── audio/            # Comunicação e callbacks da engine miniaudio
│   ├── cli/              # Logger de erros e parsing de argumentos
│   ├── core/             # Estruturas de dados (AST, SoundEvent) e tempo
│   ├── engine/           # Loop principal e lógica de agendamento (scheduler)
│   ├── frontend/         # Invólucro C++ para o Lexer e o Parser
│   └── interpreter/      # Lógica de interpretação e percurso da árvore sintática
├── tests/                # Suíte completa de testes unitários (doctest)
├── lexer/                # Analisador léxico em Flex (lexer.l)
├── parser/               # Analisador sintático em Bison (parser.y)
├── include/              # Dependências externas single-header (miniaudio.h, doctest.h)
├── samples/              # Arquivos de áudio (.wav) para percussão e instrumentos
├── exemplos/             # Scripts funcionais (.mus) para teste da linguagem
└── docs/                 # Site do projeto (GitHub Pages), manuais e atas
```

## 👥 Integrantes e Rotatividade

O desenvolvimento utiliza métodos ágeis (Scrum), e as responsabilidades pelas frentes de trabalho (Análise Léxica, Análise Sintática, Motor de Áudio, Testes e Documentação) são rotacionadas a cada Sprint. O rastreamento atual de tarefas ocorre na aba "Projects".

- Arthur Luiz (@ArthurLuizUnB)
- Caio Melo Borges (@CaioMelo25)
- Cecília Costa (@CeciliaCunha)
- Julia Oliveira (@juliapat18)
- Marcella Anderle (@marcellaanderle)
