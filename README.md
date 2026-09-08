# Interpretador Musical — Grupo 03

Interpretador que traduz uma **linguagem de programação própria em som**: um programa
escrito nessa linguagem não imprime texto, ele toca música. O lexer, o parser e o
interpretador são autorais — nenhuma engine pronta faz esse trabalho.

Trabalho da disciplina de **Compiladores 1** (Turma 01, Prof. Sérgio) — FCTE/UnB.

📄 **Site do projeto:** https://interpretador-musical.github.io/Grupo03-COMP01/

## Guia de Instalação e Execução

O projeto é escrito em C++17 e depende de ferramentas clássicas de compilação (Flex e
Bison), além da biblioteca de áudio `miniaudio` (já versionada em `include/`).

### 1. Pré-requisitos

- Compilador C++ com suporte a C++17
- **CMake** 3.16 ou superior
- **GNU Flex** (analisador léxico)
- **GNU Bison** (analisador sintático)
- Bibliotecas nativas de áudio do sistema

#### Linux (Ubuntu/Debian) e Windows via WSL

```bash
sudo apt update
sudo apt install build-essential cmake flex bison libasound2-dev libpulse-dev
```

#### macOS

```bash
brew install cmake flex bison
```

O áudio usa CoreAudio, que já vem com o sistema. O Bison do macOS é a versão 2.3
(de 2006) — ela dá conta do estado atual, mas o [CMP-03](https://github.com/Interpretador-Musical/Grupo03-COMP01/issues/15)
vai exigir Bison 3.0+, então vale instalar pelo Homebrew e apontar o CMake para ele.

### 2. Compilando

Na raiz do repositório:

```bash
cmake -S . -B build
cmake --build build -j
```

### 3. Executando

```bash
./build/compilador --help          # opções disponíveis
./build/compilador --test-tone     # senoide de 440 Hz por 3 s (valida o áudio)
./build/compilador --demo          # imprime uma timeline de exemplo
./build/compilador --loop          # roda o motor de tempo até Ctrl+C
./build/compilador programa.mus    # interpreta um programa
```

## Pipeline

```
código-fonte (.mus)
  → Flex (lexer)      → tokens
  → Bison (parser)    → AST
  → Interpretador     → lista de SoundEvent
  → miniaudio         → som
```

O interpretador **não toca som**. Ele gera uma lista de eventos temporizados
(`startTime`, `duration`, `frequency`, `volume`) que só depois é entregue ao motor de
áudio — o que mantém a lógica de linguagem separada da lógica de áudio.

O conceito central é o **playhead**: um cursor de tempo que avança conforme o programa
executa. Tocar uma nota agenda um evento na posição atual do cursor e o empurra para
frente, o que faz um laço virar padrão rítmico e uma função virar motivo musical.

## Stack

| Camada | Escolha |
|---|---|
| Implementação | C++17 |
| Análise léxica | Flex |
| Análise sintática | Bison |
| Áudio | miniaudio |
| Build | CMake |
| Plataforma alvo | Windows |

## Estrutura do repositório

```
src/
  main.cpp       entrypoint da CLI
  cli/           logger e parsing de argumentos
  audio/         motor de áudio (miniaudio)
  core/          estruturas musicais e tempo
  engine/        loop principal e thread de tempo
lexer/           lexer.l   (Flex)
parser/          parser.y  (Bison)
include/         dependências single-header (miniaudio)
docs/            site do projeto (GitHub Pages) e documentação
  atas/          atas de reunião
```

## Estado atual

O projeto está na fase de definição da linguagem. O nome, o idioma da sintaxe e a
gramática EBNF ainda não foram fechados; as decisões já tomadas estão registradas nas
atas de reunião, dentro de `docs/atas/`. A gramática que está hoje no `parser.y` é o
esqueleto de calculadora aritmética herdado do CMP-01, e será substituída assim que
essas definições fecharem.

## Integrantes

Arthur Luiz · Caio Melo Borges · Cecília Costa · Julia Oliveira · Marcella Anderle
