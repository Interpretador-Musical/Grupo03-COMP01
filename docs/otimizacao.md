---
title: Otimização (Dobra de Constantes)
layout: default
parent: Documentação
nav_order: 15
---

# Otimização: dobra de constantes

Este documento registra a otimização da etapa opcional do guia de compilador (COMP-D, #110): o que ela faz, por que ela foi escolhida, como se garante que o resultado musical não muda, e quanto ela ganha.

---

## 1. Onde fica no pipeline

```
.mus → lexer → parser → AST → análise semântica → [dobra de constantes] → interpretador → Timeline (IR)
```

O passo fica em `src/otimizacao/dobra_constantes.{h,cpp}` (biblioteca `mus_core`) e é chamado pelo `main.cpp` entre a semântica e o interpretador.

- **Depois da semântica**, porque ela precisa da árvore como foi escrita para apontar "Linha X, Coluna Y" nos erros.
- **Antes do interpretador**, porque é ele quem percorre a árvore a cada volta de `repita` e ganha com uma árvore menor.

O passo vem **ligado por padrão**. A flag `--sem-otimizacao` desliga, e serve para comparar a saída:

```bash
./build/compilador -q --emit-timeline programa.mus > com.csv
./build/compilador -q --sem-otimizacao --emit-timeline programa.mus > sem.csv
diff com.csv sem.csv   # vazio
./build/compilador -v --no-audio programa.mus   # o log DEBUG conta as dobras
```

## 2. O que a transformação faz

Toda expressão unária ou binária cujos operandos são **todos constantes** é avaliada uma vez, antes da execução, e trocada por um literal com o resultado:

| Escrito | Depois da dobra |
|---|---|
| `toca la4 - 2 por 1 / 4 * 2` | `toca 67 por 0.5` |
| `repita 4 * 4 vezes { ... }` | `repita 16 vezes { ... }` |
| `x * (2 + 3)` | `x * 5` |
| `b = nao (1 < 2) ou falso` | `b = falso` |

São constantes os literais numéricos, booleanos e strings, e as notas com oitava escrita (`la4` = 69). A dobra desce em `repita`, trilhas paralelas e todos os comandos que têm expressão.

**Por que esta otimização.** Entre as alternativas da issue, é a que tem benefício real com a linguagem como ela é hoje. O interpretador é tree-walking e avalia cada expressão de novo a cada volta de `repita`. Escrever durações e alturas como conta (`1 / 3`, `la4 - 2`, `tempo * 2`) é o jeito normal de escrever nesta linguagem, como mostram os exemplos e o `tests/ir/expressoes.mus`. Além disso, é a otimização de código intermediário clássica do material da disciplina, e é pequena o bastante para ser verificada por completo.

**Alternativas descartadas:**
- **Juntar pausas consecutivas ou remover eventos de pausa da Timeline:** mudaria o IR emitido (`--emit-timeline`) e os goldens da COMP-A/COMP-B, sem ganho audível.
- **Propagação de constantes (substituir `x` pelo valor de `x = 2`):** exige análise de fluxo, porque a variável pode ser reatribuída dentro de um `repita`. Fica como trabalho futuro (seção 6).

## 3. Como a semântica é preservada

| Garantia | Como |
|---|---|
| Valor dobrado idêntico bit a bit ao calculado na execução | O significado de cada operador foi extraído para `src/interpreter/operacoes.{h,cpp}`. O interpretador e a dobra chamam as **mesmas funções**, com as mesmas operações em `double` na mesma ordem. |
| Nada de simplificação algébrica | Só dobra quando **todos** os operandos são constantes. `x * 1`, `x + 0` e `x - x` mudariam o resultado para x = −0, NaN ou infinito, e `falso e x` pularia um erro que `x` causaria (o interpretador não faz curto-circuito). |
| Nota sem oitava não é constante | `do` depende da `oitava` corrente na execução: dentro de um `repita`, a mesma nota pode ser avaliada antes e depois de um `oitava 5`. |
| Erros iguais | Uma operação inválida (aritmética entre strings) não é dobrada, e o erro continua na execução com a mesma mensagem. Uma divisão por zero dobra para infinito, e o `toca` que a usa falha como antes. |
| Árvore consistente | O nó é alterado no lugar (o pai não precisa ser reapontado), os filhos antigos são liberados (ASan sem vazamento), a posição é mantida e a profundidade (CMP-08) é recalculada. |

## 4. Validação

Os testes estão em `tests/test_dobra_constantes.cpp`:

- **A transformação:** forma da árvore depois da dobra (literais, dobra parcial com variável, nota sem oitava intacta, booleanos, unários, `repita`, trilhas), posição e profundidade, e idempotência (uma segunda passada não dobra nada).
- **Bit a bit:** −0, ±infinito, NaN e `0.1 + 0.2` saem iguais com e sem a dobra.
- **Aleatório:** 2 000 expressões geradas com semente fixa, com profundidade até 5, os 12 operadores binários, os 2 unários, e folhas numéricas, booleanas, notas com e sem oitava e variável. O valor final é comparado bit a bit, e o tipo também.
- **Programas do repositório:** todos os `.mus` de `exemplos/`, `tests/ir/` e `tests/estresse/mista.mus`, incluindo os que falham de propósito, produzem o mesmo CSV do `--emit-timeline`, ou o mesmo erro, com e sem a dobra.
- **Ponta a ponta:** os testes `emit_timeline_*` e `ir_*` do CTest rodam o executável, agora com a dobra ligada, contra os mesmos `.csv` de antes, sem nenhum golden alterado.

## 5. Benefício medido

Para reproduzir (em Release; o caso é pulado por padrão):

```bash
cmake -S . -B build-rel -DCMAKE_BUILD_TYPE=Release
cmake --build build-rel -j --target mus_tests
./build-rel/tests/mus_tests --no-skip --test-case="dobra: medição*"
```

A carga é `repita 200000 vezes { toca la4 - 12 + 7 por 1 / 8 * 2; volume 0.5 * 2 - 0.25 }`. O tempo é só o do interpretador, o melhor de 5 execuções, medido em 4 rodadas na mesma máquina:

| | Nós de expressão no corpo | Interpretação |
|---|---:|---:|
| Sem dobra | 17 | 74,6 – 77,5 ms |
| Com dobra | 5 | 57,2 – 59,7 ms |

São **~23% a menos** de tempo de interpretação nesta carga. O ganho não é maior porque a maior parte do custo de cada volta está em criar o `SoundEvent` e inserir na Timeline, não em avaliar a expressão.

Em `tests/ir/expressoes.mus`, a árvore cai de 30 para 24 nós de expressão (`-v` mostra a contagem). O custo da passada é linear no tamanho da árvore e acontece uma vez só.

## 6. Limitações

- **Sem propagação de constantes:** `t = 0.5` seguido de `toca do4 por t * 2` não dobra, porque `t` é variável.
- **Sem reassociação:** `x + 1 + 2` é `(x + 1) + 2` e não vira `x + 3`. Em ponto flutuante, reassociar muda o resultado.
- **O ganho aparece só no tempo de compilação, não no som.** No modelo batch, a Timeline é gerada inteira antes de tocar, e o callback de áudio não é afetado.
- **Programas sem expressões constantes não ganham nada**, e a passada custa só a contagem da árvore.
