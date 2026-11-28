---
title: Sobre o projeto
layout: default
nav_order: 1
permalink: /
---

# Compilador Musical
{: .fs-9 .mb-2 }

Uma Domain-Specific Language (DSL) que transforma código em música.
{: .fs-6 .fw-300 .text-grey-dk-000 }

[Documentação]({{ site.baseurl }}{% link documentacao.md %}){: .btn .btn-primary .mr-2 }
[Roadmap]({{ site.baseurl }}{% link roadmap.md %}){: .btn .mr-2 }
[Relatório P2]({{ site.baseurl }}{% link relatorio_p2.md %}){: .btn .mr-2 }
[Relatório Final]({{ site.baseurl }}{% link relatorio_final.md %}){: .btn }

---

## O que é

Este projeto é um **compilador em lote (*batch*) que traduz uma linguagem de programação própria em som**. Em vez de imprimir texto na tela, um programa escrito nesta linguagem produz música. Todo o pipeline — desde o analisador léxico até o gerador da representação intermediária e síntese de sinal — é inteiramente autoral.

É o trabalho do Grupo 03 na disciplina de **Compiladores 1** (Turma 01, Prof. Sérgio), na FCTE/UnB.

## Por que não é só um tradutor de notas

Ao aprovar a ideia, o professor colocou uma condição: a linguagem precisa ter complexidade real para valer como trabalho de Compiladores. Não basta um tradutor linear do tipo "nota X toca por tempo Y".

Por isso a linguagem suporta:

- variáveis com tipo inferido na primeira atribuição (`tempo = 0.5` declara um `numero`);
- estruturas de repetição (`repita N vezes { ... }`);
- trilhas paralelas (`{ trilha1, trilha2 }`) para polirritmia e acordes;
- análise semântica com tabela de símbolos e escopos estáticos locais;
- expressões aritméticas e lógicas, para transposição de notas e cálculo de durações;
- comentários de linha e de bloco;
- erros léxicos, sintáticos e semânticos com indicação precisa de linha e coluna;
- representação intermediária formal (Timeline) auditável via `--emit-timeline`.

## Como funciona

```mermaid
flowchart LR
  A["código-fonte<br/>(.mus)"] --> B["Flex<br/>análise léxica"]
  B -- tokens --> C["Bison<br/>AST em Arena"]
  C -- AST --> D["Semântica<br/>tabela de símbolos"]
  D -- "AST validada" --> O["Otimização<br/>dobra de constantes"]
  O --> E["Interpretador<br/>Timeline IR"]
  E -- "-o / --saida" --> W["Gerador WAV<br/>(PCM 16-bit, COMP-C)"]
  E -- "lista de SoundEvent" --> F["AudioEngine<br/>(miniaudio)"]
  F --> G(("som"))
```

Uma decisão de arquitetura atravessa todo o projeto: **o núcleo do compilador não emite som**. Ele produz uma lista estática de eventos temporizados (`SoundEvent`, definida em `src/core/sound_event.h`) —

```cpp
struct SoundEvent {
    double startTime;            // quando começa, em segundos
    double duration;             // quanto dura, em segundos
    double frequency;            // altura da nota, em Hz (0 em pausas/samples)
    float  volume;               // amplitude, de 0.0 a 1.0
    SoundEventType type;         // Synth (nota) ou Sample (WAV)
    std::uint32_t sampleId;      // identificador no SampleBank
    float  cutoffHz;             // corte do filtro LPF BiQuad em Hz (0 = bypass, DSP-11)
};
```

— que só depois da compilação completa é convertida em arquivo `.wav` (`-o` / `--saida`) ou entregue ao motor de áudio. Isso mantém a lógica de linguagem separada da lógica de processamento de sinais, tornando o compilador testável sem depender de hardware de som.

## O playhead

O conceito central deste compilador é o **playhead**: um cursor de tempo que avança conforme o interpretador percorre a árvore sintática. Agendar uma nota não bloqueia a execução — apenas insere um evento na posição atual do cursor e o empurra para a frente.

É esse mecanismo que dá sentido musical às estruturas de controle:

| Construção | Efeito musical |
|---|---|
| Sequência de comandos | Notas em sequência, uma após a outra |
| Repetição | O cursor avança a cada volta, transformando laço em **padrão rítmico** |
| Condicional | Variação: caminhos diferentes produzem trechos diferentes (planejada) |
| Função | Um **motivo reutilizável**; o tempo gasto dentro dela se acumula no cursor externo (planejada) |
| Trilhas paralelas `{ a, b }` | **Vozes simultâneas**: cada trilha parte do mesmo instante com um cursor próprio, e a música continua do fim da mais longa |

As trilhas paralelas (CMP-07, #39 / ADR 002) são a forma de ter várias vozes ao mesmo tempo: cada uma tem um cursor independente, e os eventos de todas são intercalados por início numa timeline só antes do envio ao áudio.

## Um exemplo

{: .nota }
> A sintaxe abaixo é a **definida e implementada** no lexer, no parser, no analisador semântico e no interpretador.

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

Quatro repetições de um arpejo: o mesmo trecho de código, executado quatro vezes, transforma-se em quatro compassos sonoros (pois o playhead avança a cada nota processada).

Uma versão comentada deste programa está no repositório, em `exemplos/arpejo.mus`. Para ver o analisador léxico extraindo os tokens dele, com linha e coluna de cada um:

```bash
./build/compilador --tokens exemplos/arpejo.mus
```

Para inspecionar a tabela de símbolos e checagem semântica:

```bash
./build/compilador --simbolos exemplos/arpejo.mus
```

Para emitir a representação intermediária em CSV determinístico:

```bash
./build/compilador --emit-timeline exemplos/arpejo.mus
```

E para gerar a saída-alvo WAV offline ou compilar e tocar:

```bash
./build/compilador -o arpejo.wav exemplos/arpejo.mus # gera saída-alvo WAV (176400 frames, 4 s)
./build/compilador exemplos/arpejo.mus               # compila e toca ao vivo
./build/compilador --no-audio exemplos/arpejo.mus    # compila sem tocar: "14 eventos, 4 s de música"
```

Um erro no programa é apontado com a posição exata, e nada chega ao áudio. O `exemplos/erros_semanticos.mus` traz um erro semântico de cada tipo, e a análise recusa todos antes de gerar qualquer evento:

```text
$ ./build/compilador exemplos/erros_semanticos.mus
[ERROR] erro semântico: Linha 14, Coluna 14: 'frase' usado antes de ser declarado ou fora de escopo
[ERROR] erro semântico: Linha 17, Coluna 1: redeclaração inválida: 'tempo' é numero, recebeu booleano
[ERROR] erro semântico: Linha 20, Coluna 14: a duração do toca espera numero, recebeu nota
[ERROR] erro semântico: Linha 21, Coluna 6: operação inválida: nota + nota
[ERROR] erro semântico: Linha 24, Coluna 14: 'compasso' usado antes de ser declarado ou fora de escopo
```

### As decisões de sintaxe

| Assunto | Decisão |
|---|---|
| Idioma | Português, de ponta a ponta |
| Notas | `do re mi fa sol la si`, com `#` para sustenido e `b` para bemol |
| Oitava | Colada na nota: `do4` é o dó central (MIDI 60), `la4` é o lá de 440 Hz |
| Sem oitava | `toca do` usa a oitava corrente, definida por `oitava 4` |
| Duração | Em **tempos**: `1.0` é uma semínima, `0.5` uma colcheia, e o nome da figura também vale (`toca do4 por colcheia`). O BPM converte para segundos |
| Separador | A palavra **`por`** separa altura de duração: `toca do4 por tempo` |
| Trilhas Paralelas | `{ trilha1, trilha2 }` para vozes simultâneas e polirritmia |
| Comentários | `//` até o fim da linha e `/* */` em bloco |
| Blocos | Delimitados por `{ }` |

A palavra-chave `por` é obrigatória. Sem ela, a instrução `toca la4 - 2 tempo` seria ambígua: o parser não teria como saber se `- 2` pertence à altura ou é o começo da duração. Com o separador, os dois lados aceitam expressão à vontade e a gramática fica sem nenhum conflito de shift/reduce.

## Stack

| Camada | Escolha |
|---|---|
| Implementação | C++17 |
| Análise léxica | Flex |
| Análise sintática | Bison + Arena Allocator (`MAT-10`, teto de 256 níveis `CMP-08`) |
| Análise semântica | Tabela de Símbolos com escopos estáticos e checagem de tipos |
| Otimização | Dobra de constantes sobre a AST (`COMP-D`, `--sem-otimizacao`) |
| Código intermediário | Timeline linear de `SoundEvent`s governada por 6 invariantes |
| Saída-alvo final | WAV PCM 16-bit linear offline (44 100 Hz, mono via `src/codegen/`, `-o` / `--saida`) |
| Áudio | `miniaudio` + wavetable (`DSP-08`), filtro passa-baixa BiQuad RBJ (`DSP-11`), limitador de pico + *soft-clipper* e Volume Master (`DSP-09`) |
| Build | CMake |
| Testes | `doctest` (524 casos na suíte padrão; 537 em `mus_tests --count`) + CTest (41 testes), rodando no CI em Linux e macOS, com Cppcheck, ASan e TSan |
| Plataformas | Windows (via WSL), Linux e macOS |

## Estado atual (Sprint 06)

O compilador reúne o pipeline completo em lote (*batch*):

- **Front-end e Gramática:** `lexer.l` e `parser.y` sem conflitos shift/reduce, formalizados na [Gramática EBNF]({{ site.baseurl }}{% link gramatica_ebnf.md %}), com figuras rítmicas/`vez` (`CMP-11`), AST em arena (`MAT-10`) e limite determinístico de profundidade de 256 níveis (`CMP-08`).
- **Análise Semântica e Otimização:** verificação estática de tipos, inferência na primeira atribuição, resolução de escopo em blocos `{ }`, emissão via `--simbolos` e dobra de constantes (`COMP-D`).
- **Trilhas Paralelas:** suporte a polirritmia e acordes via `{ trilha1, trilha2 }` (`CMP-07` / [ADR 002]({{ site.baseurl }}{% link adr_002_trilhas_paralelas.md %})), com escopos isolados por trilha.
- **Representação Intermediária (IR) e Saída-Alvo:** `Timeline` formalizada com 6 invariantes (`COMP-A`), exportação determinística em CSV (`--emit-timeline`, `COMP-B`) e geração de código final em **WAV PCM 16-bit** offline (`-o` / `--saida`, `COMP-C`).
- **Áudio:** fila SPSC *lock-free*, síntese por wavetable (`DSP-08`), samples WAV (`INT-05`), envelopes adaptativos de até 5 ms (#123), filtro passa-baixa BiQuad RBJ por voz e no master bus (`DSP-11`), limitador de pico + *soft-clipper* (`BUG-AUDIO` #109, `DSP-10`) e Volume Master suavizado (`DSP-09`).
- **Garantia de Qualidade e CI:** `524` casos na suíte padrão (`537` em `mus_tests --count`) e `41` testes no CTest, incluindo integração ponta a ponta (`INT-06`), estresse (`INT-07`), auditoria estática Real-Time (`SCH-11`), portabilidade de `samples/` (`INT-08`) e validação CLI de saída WAV. AddressSanitizer, ThreadSanitizer e Cppcheck rodam em todo PR.

Veja o [Manual do Usuário]({{ site.baseurl }}{% link manual_usuario.md %}) para programar em `.mus` e a [Visão de Arquitetura]({{ site.baseurl }}{% link arquitetura.md %}) para o pipeline completo. As evidências consolidadas estão disponíveis no [Relatório Final]({{ site.baseurl }}{% link relatorio_final.md %}) e no [Relatório P2]({{ site.baseurl }}{% link relatorio_p2.md %}), o roteiro de apresentação e release em [Release Final e Demonstração]({{ site.baseurl }}{% link release_final.md %}), as decisões arquiteturais nas [ADRs]({{ site.baseurl }}{% link documentacao.md %}) e a situação das entregas no [Roadmap]({{ site.baseurl }}{% link roadmap.md %}).
