---
title: "Ata — 19/08/2026"
layout: default
parent: Documentação
nav_order: 1
---

# Ata de Reunião — 19/08/2026

- **Projeto:** Interpretador de linguagem de programação musical
- **Disciplina:** Compiladores 1 — Turma 01 (Prof. Sergio) — FCTE/UnB
- **Horário:** *(a confirmar)*
- **Local / formato:** *(a confirmar)*

## Participantes

- Arthur Luiz
- Caio Melo Borges
- Cecília Costa
- Julia Oliveira
- Marcella Anderle

## Pauta

1. Apresentação da ideia do projeto ao grupo e ao professor
2. Retorno do professor sobre a viabilidade e o escopo mínimo exigido
3. Escolha da linguagem de implementação
4. Avaliação do uso da engine Godot / GDScript
5. Escolha da biblioteca de áudio
6. Definição da arquitetura do pipeline e do conceito de *playhead*
7. Levantamento das features que a linguagem precisa suportar

## Discussões e decisões

### 1. Ideia do projeto

Foi apresentada a proposta de construir um **interpretador que traduz uma linguagem
de programação customizada em som**, permitindo compor música escrevendo código —
conceito próximo ao de ferramentas como o Sonic Pi. O grupo aderiu à ideia como tema
do trabalho da disciplina.

### 2. Aprovação do professor e ressalva sobre complexidade

A ideia foi **apresentada e aprovada pelo professor**, que destacou que nenhum grupo
da turma havia proposto algo semelhante antes.

Foi feita, porém, uma **ressalva importante**: a linguagem precisa ter complexidade
real para valer como trabalho de Compiladores. Não basta um tradutor linear do tipo
"nota X toca por tempo Y" — é necessário que a linguagem tenha estruturas de
repetição, condicionais, variáveis e funções. Essa ressalva passa a ser o critério
que orienta o desenho da linguagem daqui em diante.

### 3. Linguagem de implementação: C++

**Decisão:** o interpretador será implementado em **C++ (padrão C++17)**.

**Build system:** **CMake**.
**Plataforma alvo:** **Windows**.

### 4. Por que não usar Godot / GDScript

Foi cogitado usar a engine **Godot** para facilitar as partes gráfica e de áudio.
A opção foi **descartada como abordagem principal** pelo seguinte motivo: escrever o
projeto em GDScript significaria depender do parser e do interpretador já prontos da
engine, o que **enfraqueceria justamente a parte de "compiladores" do trabalho**, que
é o núcleo da avaliação.

**Decisão:** seguir com C++ puro. Ficou registrada como **alternativa futura não
prioritária** a possibilidade de usar o Godot apenas como motor de áudio (via
GDExtension), mantendo o parser e o interpretador 100% autorais.

### 5. Biblioteca de áudio: miniaudio

**Decisão:** usar a **`miniaudio`** (single-header). Motivos levantados:

- é *single-header*, o que simplifica muito a integração e o build;
- permite **gerar formas de onda diretamente**, sem depender de arquivos de áudio
  prontos — ou seja, o som é sintetizado a partir de frequência/duração calculadas
  pelo interpretador.

### 6. Arquitetura do pipeline

Ficou definida a arquitetura geral do projeto:

```
código-fonte (.mus)
  → Lexer      → tokens
  → Parser     → AST
  → Interpretador (tree-walking)
  → lista de SoundEvent (timestamp, duração, frequência, volume)
  → miniaudio  → som
```

**Princípio de separação (decisão de design):** o interpretador **não toca som
diretamente**. Ele apenas produz uma lista de eventos temporizados:

```cpp
struct SoundEvent {
    float startTime;
    float duration;
    float frequency;
    float volume;
};
```

Essa lista é depois entregue ao motor de áudio. A separação entre a **lógica de
linguagem** (lexer/parser/interpretador) e a **lógica de áudio** (miniaudio) deve
ficar explícita tanto na arquitetura do código quanto na documentação e na
apresentação ao professor.

### 7. Conceito central: o *playhead* (cursor de tempo)

Discutiu-se o que diferencia este interpretador de um interpretador comum: cada
operação de "tocar uma nota" **não executa instantaneamente** — ela agenda um evento
na timeline e **avança um cursor de tempo (playhead)**.

É esse mecanismo que torna as estruturas de controle musicalmente coerentes:

- **Sequência de comandos:** cada comando de tocar agenda um evento no tempo atual do
  cursor e avança o cursor pela duração da nota.
- **Repetição (`for` / `repita ... vezes`):** o corpo é reavaliado N vezes de forma
  convencional, mas como cada iteração contém uma chamada de tocar nota, o cursor
  avança a cada volta — o que transforma repetição de código em **padrão rítmico**.
- **Condicional (`se` / `senao`):** funciona de forma convencional, e fica
  particularmente interessante quando combinada com aleatoriedade, gerando variações
  musicais.
- **Funções:** tornam-se **motivos musicais reutilizáveis** — o tempo consumido dentro
  da função se acumula no cursor externo quando ela retorna.
- **Trilhas paralelas (feature avançada, opcional):** múltiplas vozes simultâneas
  exigiriam **múltiplos cursores de tempo independentes**, com merge final das
  timelines antes do envio ao áudio. Marcada como item opcional, não prioritário.

### 8. Features que a linguagem precisa suportar

Levantamento do escopo mínimo da linguagem, derivado da ressalva do professor:

- Variáveis (ex.: tempo, tom)
- Estruturas de repetição (`for` / `repita ... vezes`)
- Condicionais (`se` / `senao`)
- Funções (declaração e chamada, com parâmetros)
- Expressões aritméticas (transposição de notas, cálculo de duração etc.)
- Comando de tocar nota (nota + duração, possivelmente volume)
- Comentários de linha e de bloco
- Tratamento de erros léxicos e sintáticos, com indicação de **linha e coluna**

## Pendências / próximos passos

- [ ] Definir o **nome da linguagem**
- [ ] Decidir se a **sintaxe será em português ou em inglês** (e manter consistência
      em toda a linguagem)
- [ ] Escrever a **gramática EBNF** completa e formalizada
- [ ] Definir a abordagem de implementação do **lexer e do parser**
- [ ] Definir as **classes/structs da AST** em C++
- [ ] Estruturar as pastas do repositório de desenvolvimento
- [ ] Dividir tarefas entre os integrantes do grupo
