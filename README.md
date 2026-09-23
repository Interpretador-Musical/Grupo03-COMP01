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
  frontend/      invólucro C++ do lexer e do parser (tokenize, parseString)
lexer/           lexer.l   (Flex)
parser/          parser.y  (Bison)
exemplos/        programas .mus de exemplo
include/         dependências single-header (miniaudio)
docs/            site do projeto (GitHub Pages) e documentação
  atas/          atas de reunião
```

## A linguagem

Sintaxe em português. Notas em `do re mi`, com `#` para sustenido e `b` para bemol. A
oitava vem colada na nota — `do4` é o dó central (MIDI 60) e `la4` é o lá de 440 Hz —, e
quando é omitida vale a oitava corrente. A duração é medida em **tempos**, convertidos em
segundos pelo andamento: `1.0` é uma semínima.

```text
andamento 120
tempo = 0.5

repita 4 vezes {
    toca do4  por tempo
    toca mi4  por tempo
    toca sol4 por tempo
}

toca la4 - 2 por tempo * 2   /* transposição e duração calculadas */
pausa por 1.0
```

A palavra `por` separa a altura da duração. Sem ela, `toca la4 - 2 por tempo` seria
ambíguo: o parser não saberia se `- 2` pertence à altura ou começa a duração. Com o
separador, os dois lados aceitam expressão e a gramática fica sem conflito nenhum.

## Estado atual

O front-end existe. O `lexer.l` reconhece o vocabulário completo — palavras-chave, notas
com acidente e oitava, inteiros e reais como tipos distintos, identificadores, operadores
e comentários de linha e de bloco — registrando linha e coluna de cada token. O `parser.y`
reconhece atribuição, os comandos musicais e a repetição com bloco.

Ainda não existem a árvore sintática, o interpretador que a percorre nem a ligação com o
motor de áudio. O nome da linguagem também segue em aberto — é decisão de reunião e não
aparece em nenhum arquivo de código.

## Integrantes

Arthur Luiz · Caio Melo Borges · Cecília Costa · Julia Oliveira · Marcella Anderle
