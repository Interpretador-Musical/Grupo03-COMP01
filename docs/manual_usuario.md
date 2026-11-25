---
title: Manual do Usuário
layout: default
parent: Documentação
nav_order: 20
---

# Manual do Usuário — Linguagem Musical `.mus`

Este manual ensina passo a passo como escrever programas musicais na linguagem `.mus`, compilar e reproduzir músicas no terminal, exportar arquivos `.wav` e diagnosticar erros comuns. Todas as construções descritas abaixo correspondem ao compilador integrado na `main` (e serão marcadas explicitamente quando estiverem reservadas para trabalho futuro).

---

## 1. Como funciona o Compilador Musical

O Compilador Musical trabalha em **modo lote (*batch*)**: você escreve um arquivo de texto `.mus` representando uma peça musical finita; o compilador analisa o arquivo inteiro, verifica a gramática e os tipos, constrói a linha do tempo de eventos (`Timeline`) e só então toca o áudio ou exporta o arquivo WAV.

O conceito central da linguagem é o **cursor temporal (*playhead*)**:

- Ao iniciar o programa, o cursor está no instante `0` (início da peça), com andamento padrão de `120 BPM`, oitava corrente `4` e volume `1.0`.
- Cada comando `toca` ou `pausa` agenda o evento na posição atual do cursor e **avança o cursor** pela duração pedida. Por isso, comandos escritos em sequência tocam **um após o outro**.
- Para fazer vários sons tocarem **ao mesmo tempo**, você usa um grupo de **trilhas paralelas** (`{ trilha1, trilha2 }`).

### Seu primeiro programa

Crie um arquivo `ola_musica.mus`:

```mus
// Minha primeira melodia em .mus
andamento 120
volume 0.8

toca do4 por 1
toca mi4 por 1
toca sol4 por 1
toca do5 por 2
pausa por 1
```

Para compilar e executar a partir da raiz do repositório:

```bash
# Compilar e tocar no dispositivo de áudio
./build/compilador ola_musica.mus

# Validar sintaxe, semântica e Timeline sem abrir o áudio
./build/compilador --no-audio ola_musica.mus

# Gerar arquivo WAV (PCM 16-bit, 44,1 kHz, mono) sem tocar na hora
./build/compilador -o ola_musica.wav ola_musica.mus
```

---

## 2. Regras Gerais, Comentários e Notas Musicais

### Espaçamento e comentários

- **Sem ponto e vírgula:** comandos não usam `;` no fim da linha (escrever `;` gera erro de sintaxe).
- **Quebras de linha livres:** você pode quebrar ou juntar comandos na mesma linha à vontade (`toca do4 por 1 toca re4 por 1` é válido).
- **Comentários de linha:** começam com `//` e vão até o fim da linha.
- **Comentários de bloco:** ficam entre `/*` e `*/` (não se aninham).

### Notação de alturas (notas e oitavas)

A linguagem usa o sistema latino em ASCII (sem acentos no nome das notas):

| Nota | Nome na linguagem |
|---|---|
| Dó | `do` |
| Ré | `re` |
| Mi | `mi` |
| Fá | `fa` |
| Sol | `sol` |
| Lá | `la` |
| Si | `si` |

- **Acidentes:** adicione `#` para sustenido e `b` para bemol logo após a nota (ex.: `fa#`, `sib`, `do#5`, `mib3`).
- **Oitava colada à nota:** anexe um dígito de `0` a `9` ao final (`do4` = Dó central, MIDI 60; `la4` = 440 Hz).
- **Nota sem oitava explícita:** quando o dígito é omitido (`toca do por 1`, `toca fa# por 0.5`), o compilador usa a **oitava corrente** configurada pelo comando `oitava` (padrão `4`).

Exemplo (baseado em `exemplos/escala.mus`):

```mus
// Escala de Dó maior ascendente e uso de acidentes
andamento 100
duracao = 1.0

toca do4  por duracao
toca re4  por duracao
toca mi4  por duracao
toca fa4  por duracao
toca sol4 por duracao
toca la4  por duracao
toca si4  por duracao
toca do5  por duracao

toca fa#4 por 0.5    // Fá sustenido 4
toca sib3 por 0.5    // Si bemol 3
```

> **Cuidado com nomes reservados:** como `do`, `re`, `mi`, `fa`, `sol`, `la`, `si` (e combinações como `fab` ou `sol4`) são tokens de nota, não podem ser usados como nomes de variável (`fab = 1` dá erro de sintaxe).

---

## 3. Tocando Notas, Pausas e Figuras Rítmicas (`toca`, `pausa`, `por`)

Os dois comandos que produzem eventos na linha do tempo são:

```text
toca <altura> por <duracao>
pausa por <duracao>
```

A palavra-chave **`por` é obrigatória**: é ela quem separa a expressão da altura da expressão da duração, evitando ambiguidade em trechos como `toca la4 - 2 por 0.5 * 2`.

As durações são sempre expressas em **tempos (*beats*)**, convertidas para segundos pelo andamento (`segundos = tempos * 60 / BPM`). Você pode escrever durações de três formas:

1. **Números inteiros ou reais:** `1`, `0.5`, `2.0`. *(Atenção: reais exigem dígito dos dois lados do ponto — `0.5` é válido, `.5` e `1.` são erros.)*
2. **Frações / divisões:** `1 / 3` (tercina), `1 / 4` (semicolcheia), `3 / 2`.
3. **Figuras rítmicas (açúcar sintático):** nomes tradicionais que equivalem diretamente ao número de tempos da tabela abaixo (com ou sem acento):

| Figura | Tempos | Equivalente numérico |
|---|---:|---|
| `semibreve` | `4` | `4.0` |
| `minima` / `mínima` | `2` | `2.0` |
| `seminima` / `semínima` | `1` | `1.0` |
| `colcheia` | `1/2` | `0.5` |
| `semicolcheia` | `1/4` | `0.25` |
| `fusa` | `1/8` | `0.125` |
| `semifusa` | `1/16` | `0.0625` |

Como as figuras viram números já na análise léxica, você pode combiná-las em expressões (por exemplo, `semínima + colcheia` para uma semínima pontuada, ou `seminima / 3` para uma tercina):

```mus
andamento 90

toca do4  por semínima
toca mi4  por colcheia
toca sol4 por semínima + colcheia   // 1.5 tempo (semínima pontuada)
pausa     por colcheia

repita 3 vezes {
    toca do5 por seminima / 3       // tercina: 3 notas em 1 tempo
}

pausa por mínima
toca do4 por semibreve
```

---

## 4. Comandos de Estado Musical (`andamento`, `oitava`, `volume`)

Estes três comandos atualizam o estado de compilação a partir do ponto em que aparecem (sem afetar eventos já agendados antes deles):

| Comando | Valor padrão | Faixa aceita | O que faz |
|---|---|---|---|
| `andamento <bpm>` | `120` | `> 0` (finito) | Define a velocidade em batidas por minuto (BPM). Mudanças no meio da música mantêm intactos os instantes em segundos das notas anteriores. |
| `oitava <n>` | `4` | `-1` a `9` (truncado para inteiro) | Define a oitava usada por notas escritas sem dígito (`do`, `re`, `fa#`). |
| `volume <ganho>` | `1.0` | `0.0` a `1.0` | Define a intensidade das notas seguintes (`0.0` = mudo, `1.0` = máximo). |

Exemplo combinando os três comandos:

```mus
andamento 100
oitava 3
volume 0.4

toca do por colcheia
toca mi por colcheia
toca sol por colcheia

// Sobe uma oitava, aumenta o volume e acelera o andamento
oitava 5
volume 0.9
andamento 140

toca do por semínima
toca re por semínima
toca mi por mínima
```

---

## 5. Variáveis, Tipos e Expressões

### Declaração e tipos

Não existe palavra-chave para declarar variáveis: **a primeira atribuição (`nome = valor`) declara a variável** no escopo atual e fixa o seu tipo por inferência. A linguagem possui quatro tipos estáticos:

| Tipo | Exemplos de valores | Onde é usado |
|---|---|---|
| `numero` | `0.5`, `120`, `-2`, `colcheia`, `la4 - do4` | Durações, quantidades do `repita`, `andamento`, `oitava`, `volume`, ou altura em número MIDI (`60` = `do4`). |
| `nota` | `do4`, `sib3`, `la4 - 2`, `do` | Altura no comando `toca` e cálculos de transposição/intervalo. |
| `booleano` | `verdadeiro`, `falso`, `1 < 2`, `nao falso` | Expressões lógicas e comparações. |
| `string` | `"PERC_09"`, `"BASS_1"` | Nome de sample WAV da pasta `samples/` no comando `toca`. |

**Regras de tipagem:**

- **Sem troca de tipo:** depois que `tempo = 0.5` cria um `numero`, escrever `tempo = verdadeiro` é **erro semântico** (*redeclaração inválida*).
- **Uso antes de declarar:** ler uma variável não declarada no escopo visível é **erro semântico**.
- **Identificadores reservados:** além dos comandos e notas, nomes de figuras rítmicas (`colcheia`, `semínima`, etc.) e palavras futuras (`se`, `entao`, `senao`, `defina`, `retorna`) não podem ser usados como nomes de variável. `tempo` e `bpm` são livres e podem ser usados normalmente.

### Expressões e operações permitidas

| Operação | Tipos aceitos | Tipo resultante | Significado |
|---|---|---|---|
| `+`, `-`, `*`, `/` | `numero` e `numero` | `numero` | Aritmética usual |
| `-x` (menos unário) | `numero` | `numero` | Negação numérica |
| `nota + numero`, `numero + nota` | `nota` e `numero` | `nota` | Transposição para cima em semitons (aceita frações/microtons, ex.: `la4 + 0.5`) |
| `nota - numero` | `nota` e `numero` | `nota` | Transposição para baixo em semitons (`la4 - 2` → `sol4`) |
| `nota - nota` | `nota` e `nota` | `numero` | Intervalo em semitons entre duas notas (`sol4 - do4` → `7`) |
| `==`, `!=` | Mesmo tipo (`numero`, `nota`, `booleano` ou `string`) | `booleano` | Igualdade / diferença |
| `<`, `>`, `<=`, `>=` | Dois `numero` ou duas `nota` | `booleano` | Comparação de ordem |
| `e`, `ou`, `nao` (ou `não`) | `booleano` | `booleano` | Conjunção, disjunção e negação lógica |

**Precedência (da mais baixa para a mais alta):**

1. `ou` (esquerda)
2. `e` (esquerda)
3. `nao` / `não` (prefixo)
4. `==`, `!=`, `<`, `>`, `<=`, `>=` (**não associativos**: `1 < 2 < 3` é erro de sintaxe)
5. `+`, `-` (esquerda)
6. `*`, `/` (esquerda)
7. `-` unário (prefixo)

Parênteses `( ... )` alteram o agrupamento normalmente.

Exemplo com variáveis, transposição e duração calculada:

```mus
andamento 120
pulso = colcheia
notaBase = do4
terca = notaBase + 4       // mi4
quinta = notaBase + 7      // sol4
intervalo = quinta - notaBase   // numero (7)

toca notaBase por pulso
toca terca    por pulso
toca quinta   por pulso * 2
toca 60 + intervalo por pulso * 2   // 60 + 7 = MIDI 67 (sol4)
```

---

## 6. Blocos de Repetição e Escopo Léxico (`repita ... vezes { ... }`)

Para repetir um trecho sem copiar comandos manualmente, use:

```text
repita <quantidade> vezes {
    <comandos>
}
```

Também é aceito `vez` no singular (ex.: `repita 1 vez { ... }`).

Regras importantes do `repita`:

- **O cursor avança a cada volta:** como o *playhead* não volta atrás após cada iteração, `repita 4 vezes { toca do4 por 0.5 }` produz quatro notas em sequência (2 tempos no total), e não quatro notas juntas.
- **Quantidade dinâmica truncada:** a expressão de voltas é avaliada antes do laço e truncada para inteiro (`repita 2.9 vezes` executa 2 vezes). Valores negativos, `NaN` ou infinitos são recusados em tempo de compilação/execução.
- **Escopo de bloco `{ ... }`:**
  - Variáveis criadas **dentro** do bloco existem apenas dentro dele (são destruídas ao sair do `}`).
  - Variáveis criadas **fora** podem ser lidas e atualizadas dentro do bloco; como o corpo é reavaliado a cada volta, reatribuir uma variável externa muda as iterações seguintes!

Exemplo gerando uma escala cromática com variável reatribuída no laço:

```mus
andamento 120
volume 0.8

passo = colcheia
notaAtual = do4

// A cada volta, toca a nota atual e sobe 1 semitom
repita 8 vezes {
    toca notaAtual por passo
    notaAtual = notaAtual + 1
}

// Laço aninhado com variável local ao bloco interno
repita 2 vezes {
    durLocal = semicolcheia
    toca do5 por durLocal
    toca sol4 por durLocal
}
```

---

## 7. Trilhas Paralelas: Acordes e Polirritmia (`{ trilha1, trilha2, ... }`)

Para fazer duas ou mais linhas musicais soarem **simultaneamente**, use chaves com **vírgula** separando cada trilha:

```text
{
    <comandos da trilha 1>,
    <comandos da trilha 2>,
    ...
}
```

> **Diferença entre bloco e grupo:** `{ ... }` *sem vírgula* só aparece depois de `repita N vezes`. Chaves *com pelo menos uma vírgula* formam um **grupo de trilhas paralelas** (inclusive dentro de um `repita`).

### Como o tempo e o escopo funcionam nas trilhas

1. **Início simultâneo:** todas as trilhas começam no mesmo instante de entrada do grupo.
2. **Durações absolutas:** `por` continua valendo em tempos sem nenhuma reescala automática (ex.: `1 / 3` contra `1 / 4` no mesmo tempo).
3. **Alinhamento ao final:** o grupo inteiro dura o tempo da **trilha mais longa**, e os comandos seguintes ao `}` continuam a partir desse ponto.
4. **Isolamento de estado:** alterações de `oitava`, `volume` ou variáveis criadas dentro de uma trilha **não vazam** para outras trilhas nem alteram o estado após o grupo.
5. **Restrições semânticas:** dentro de um grupo paralelo é proibido usar `andamento` (todas as trilhas compartilham o mesmo pulso) e proibido reatribuir variáveis declaradas fora do grupo.

Exemplo reunindo polirritmia, acorde e melodia com acompanhamento (baseado em `exemplos/polirritmia.mus`):

```mus
andamento 90

// Polirritmia 3 contra 4 em 1 tempo, repetida 2 vezes
repita 2 vezes {
    repita 3 vezes { toca do4 por 1 / 3 },
    repita 4 vezes { toca sol3 por 1 / 4 }
}

// Acorde de Dó maior (três trilhas de uma nota só)
{ toca do4 por 2, toca mi4 por 2, toca sol4 por 2 }

// Melodia aguda acompanhada de nota pedal no baixo
{
    oitava 5
    volume 0.9
    toca do por 1
    toca re por 1
    toca mi por 2,

    volume 0.5
    toca do2 por 4
}

// Fora do grupo, oitava e volume voltaram aos valores originais (4 e 1.0)
toca do por 1
```

---

## 8. Samples de Áudio WAV (`toca "NOME" por <duracao>`)

Além da síntese senoidal de notas, `toca` aceita uma **`string`** com o nome de um arquivo `.wav` existente na pasta `samples/` do repositório (sem a extensão `.wav`, ex.: `PERC_01`..`PERC_56` e `BASS_1`..):

```mus
andamento 110

// Nome literal ou guardado em variável do tipo string
bumbo = "BASS_1"
caixa = "PERC_09"

repita 4 vezes {
    {
        toca bumbo por colcheia
        toca caixa por colcheia,

        toca do4 por colcheia
        toca sol4 por colcheia
    }
}
```

Observações sobre samples:

- A string pode ser guardada em variável e comparada com `==` ou `!=`; operações aritméticas com strings são erro semântico.
- Se o sample parar antes do fim da duração pedida no `por`, o restante da duração é completado em silêncio; se o `.wav` for mais longo que a duração pedida, o evento é encerrado com *release* ao fim da duração musical.
- Um nome inexistente em `samples/` é recusado com erro de execução antes de gerar áudio ou arquivo WAV.

---

## 9. Funções, Passagem de Argumentos e Escopo

Esta seção explica o estado atual da linguagem `.mus` e da arquitetura C++ subjacente para reutilização parametrizada de trechos musicais.

### Na sintaxe `.mus` (estado atual)

As palavras-chave **`defina`** e **`retorna`** (assim como `se`, `entao`/`então` e `senao`/`senão`) já são reservadas pelo analisador léxico (`lexer/lexer.l`), mas **ainda não possuem regras ativas no parser (`parser/parser.y`)** — escrever `defina motivo(x) { ... }` em um arquivo `.mus` hoje produz erro de sintaxe.

Enquanto a sintaxe de funções não é exposta na DSL, a parametrização de motivos musicais em arquivos `.mus` é feita combinando **variáveis de entrada** com **blocos `repita` e trilhas paralelas**:

```mus
andamento 120

// Parâmetros do motivo definidos antes da execução do bloco
notaInicial = do4
duracaoNota = colcheia

repita 2 vezes {
    toca notaInicial     por duracaoNota
    toca notaInicial + 4 por duracaoNota
    toca notaInicial + 7 por duracaoNota * 2
}

// Reutilizando o mesmo padrão em outra tonalidade (Sol maior)
notaInicial = sol4
duracaoNota = semicolcheia

repita 2 vezes {
    toca notaInicial     por duracaoNota
    toca notaInicial + 4 por duracaoNota
    toca notaInicial + 7 por duracaoNota * 2
}
```

### Na arquitetura C++ (`MAT-06`: Closures e Passagem de Argumentos)

Embora a sintaxe `.mus` ainda não exponha `defina`/`retorna`, o motor de interpretação em C++ já implementa a infraestrutura completa de escopo léxico, captura de ambiente e passagem de argumentos (`src/interpreter/ambiente.h` e `src/interpreter/interpreter.h`):

1. **`mus::Ambiente`** mantém uma pilha de mapas de variáveis (`escopos_`), com busca do escopo mais interno até o global e guarda RAII (`mus::EscopoGuard`) para fechamento seguro de escopos.
2. **`mus::Closure`** captura por valor o snapshot do ambiente visível no momento da declaração (`Closure::capturarBloco` / `Closure::capturar`), armazenando a lista de nomes de `parametros` e o corpo da AST (`corpoAst`).
3. **`mus::Interpretador::declararClosure(nome, parametros, corpoAst)`** registra a função capturando o escopo atual.
4. **`mus::Interpretador::invocarClosure(nome, argumentos)`** valida se a função foi declarada e se a aridade de `argumentos` coincide com `parametros.size()`, monta um ambiente isolado com o estado capturado, abre um novo escopo vinculando cada argumento posicional ao nome do parâmetro correspondente (sombreando variáveis externas de mesmo nome) e executa o corpo da AST acumulando os eventos na `Timeline`:

```cpp
// Exemplo de uso da API C++ (ver tests/test_interpreter.cpp)
auto corpo = mus::parseToAst("toca nota por dur");
mus::Interpretador interp;
interp.declararClosure("motivo", {"nota", "dur"}, corpo.get());
interp.invocarClosure("motivo", {mus::Valor{60.0}, mus::Valor{0.5}});
```

---

## 10. Guia Completo da CLI e Diagnóstico de Erros

### Opções de linha de comando (`compilador`)

| Opção | Descrição |
|---|---|
| `-h`, `--help` | Exibe a ajuda completa de uso e sai. |
| `--version` | Exibe a versão do compilador e sai. |
| `-v`, `--verbose` | Ativa mensagens de `DEBUG` no log (`stderr`). |
| `-q`, `--quiet` | Exibe apenas erros no log (`stderr`). |
| `--tokens <arq.mus>` | Executa apenas a análise léxica e imprime a tabela de tokens (linha, coluna, tipo, lexema e valor). |
| `--simbolos <arq.mus>` | Executa até a análise semântica e imprime a tabela de símbolos (endereço, nome, tipo, escopo, declaração e usos). |
| `--emit-timeline <arq.mus>` | Compila integralmente e imprime a Representação Intermediária (`Timeline`) em CSV determinístico no `stdout`, sem tocar som. |
| `-o`, `--saida <arq.wav>` | Compila integralmente e gera o arquivo WAV PCM 16-bit (44 100 Hz, mono) offline, sem abrir placa de som. |
| `--sem-otimizacao` | Desliga a passagem de dobra de constantes (`COMP-D`) para fins de comparação. |
| `--no-audio` | Compila e valida o `.mus` sem inicializar o dispositivo de áudio. |
| `--demo` | Imprime uma `Timeline` de demonstração sem precisar de `.mus`. |
| `--bpm <n>` | Define o andamento usado pelo `--demo` (padrão: `120`). |
| `--loop` | Mantém o motor de tempo rodando até `Ctrl+C`. |
| `--test-tone` | Toca uma senoide de teste de 440 Hz por 3 segundos e sai. |

> **Combinações inválidas:** `--emit-timeline` e `-o`/`--saida` não podem ser combinados com `--tokens` ou `--simbolos`; `-o`/`--saida` também não se combina com `--demo`, `--loop` ou `--test-tone`.

### Erros comuns e como corrigi-los

| Fase | Exemplo de erro | Causa e solução |
|---|---|---|
| **Léxico / Sintático** | `Linha 1, Coluna 9: syntax error` | Faltou `por` em `toca do4 0.5`, usou `;` no fim do comando, escreveu `.5` sem zero antes, ou usou nome reservado (`fab = 1`, `colcheia = 2`). |
| **Profundidade AST** | `aninhamento passa do limite de 256 níveis` | Expressão ou blocos aninhados ultrapassaram 256 níveis. Quebre expressões gigantes em variáveis intermediárias. |
| **Semântica — Não declarado** | `'frase' usado antes de ser declarado ou fora de escopo` | Variável usada antes de receber valor, ou declarada apenas dentro de um `repita` / outra trilha. Declare-a antes do uso ou no escopo adequado. |
| **Semântica — Tipo** | `redeclaração inválida: 'tempo' é numero, recebeu booleano` | Uma variável não pode mudar de tipo após a primeira atribuição. Use outro nome para o novo tipo. |
| **Semântica — Trilhas** | erro ao alterar variável externa ou usar `andamento` em `{ , }` | Mova `andamento` para fora do grupo `{ , }` e use variáveis locais ou constantes dentro das trilhas. |
| **Execução — Valores** | `volume precisa estar entre 0 e 1` / `oitava precisa estar entre -1 e 9` / `andamento invalido` | Ajuste os valores calculados para as faixas válidas e evite divisões por zero em durações ou contadores. |
| **Execução — Limites** | `passa do limite de 5000000 eventos` ou `10000000 voltas` | Reduza o número de iterações nos laços `repita`. |
