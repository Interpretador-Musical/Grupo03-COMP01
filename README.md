# Interpretador Musical — Grupo 03

Interpretador que traduz uma **linguagem de programação própria em som**: um programa
escrito nessa linguagem não imprime texto, ele toca música. O lexer, o parser e o
interpretador são autorais — nenhuma engine pronta faz esse trabalho.

Trabalho da disciplina de **Compiladores 1** (Turma 01, Prof. Sérgio) — FCTE/UnB.

📄 **Site do projeto:** https://interpretador-musical.github.io/Grupo03-COMP01/

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
docs/          site do projeto (GitHub Pages) e documentação
  atas/        atas de reunião
```

O restante (`lexer/`, `parser/`, `src/`, `examples/`) entra conforme a implementação
começar — ver o roadmap no site.

## Estado atual

O projeto está na fase de definição da linguagem. O nome, o idioma da sintaxe e a
gramática EBNF ainda não foram fechados; as decisões já tomadas estão registradas nas
atas de reunião, dentro de `docs/atas/`.

## Integrantes

Arthur Luiz · Caio Melo Borges · Cecília Costa · Julia Oliveira · Marcella Anderle
