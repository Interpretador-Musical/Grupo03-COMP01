---
title: "ADR 002: Trilhas Paralelas Finitas"
layout: default
parent: Documentação
nav_order: 12
---

# ADR 002 — Polirritmia de bloco no compilador batch

| Campo | Valor |
|---|---|
| Status | **Implementada e integrada** — CMP-07 #39, PR #128 |
| Data da decisão | 02/11/2026 |
| Contexto anterior | [ADR 001: Modelo Batch]({{ site.baseurl }}{% link decisao_modelo_batch.md %}) |
| Implementação | `parser/parser.y`, `src/interpreter/ast.*`, `src/interpreter/interpreter.*`, `src/semantic/analisador_semantico.*` |
| Evidências | `tests/test_trilhas.cpp`, `tests/ir/trilhas.mus`, `tests/emit_timeline/polirritmia.csv`, `exemplos/polirritmia.mus` |

## 1. Problema

O projeto precisava definir o comportamento de duas ou mais trilhas executadas no mesmo intervalo. O planejamento inicial tomava a sintaxe `{ , }` do TidalCycles, onde os padrões podem continuar indefinidamente; o compilador atual, porém, só aceita **programas musicais finitos**. O grupo optou por uma semântica simples e verificável, sem reescalas proporcionais.

## 2. Decisão

Adotar **trilhas paralelas finitas** ou **polirritmia de bloco**: `{ trilha1, trilha2, ... }` é um comando com duas ou mais trilhas, todas com o mesmo instante inicial.

- Cada `toca`/`pausa` usa seu próprio `por` em beats, sem divisão proporcional nem sincronização artificial de término.
- O grupo dura até o fim da trilha que terminar por último.
- Depois do grupo, a música continua exatamente desse instante final.
- O estado de oitava e volume é restaurado após cada trilha e ao final do grupo.
- `andamento` dentro do grupo e reatribuir variáveis externas ao grupo são erros semânticos.
- Cada trilha possui escopo separado na análise semântica. A infraestrutura de ambientes léxicos da MAT-06 foi integrada pelo PR #129. A compatibilidade entre os ambientes de execução e os grupos de trilhas paralelas deve ser preservada e verificada por testes de regressão.
- Grupos podem ser aninhados e usados no corpo de `repita`. Trilhas vazias são aceitas.
- A ordenação da Timeline por `startTime` é estável, preservando a ordem dos eventos simultâneos de um mesmo programa.

## 3. Exemplo e cálculo independente

```mus
andamento 120
{
    repita 3 vezes { toca do4 por 1 / 3 },
    repita 4 vezes { toca sol3 por 1 / 4 }
}
toca do5 por 1
```

Em 120 BPM, um beat dura 0,5 segundo. A primeira trilha produz três eventos de 1/3 beat; a segunda, quatro eventos de 1/4 beat. Ambas totalizam um beat. O `toca do5` seguinte começa no instante 0,5 segundo.

Se uma trilha durasse 2 beats e outra 1 beat, o grupo duraria **2 beats**, sem estender nem reescalar a trilha curta.

## 4. Alternativas rejeitadas

| Alternativa | Motivo |
|---|---|
| Polimetria contínua com padrões infinitos | Exigiria modelo de execução e consulta temporal diferente do batch |
| Reescalar cada trilha para um ciclo comum | Alteraria a semântica única de duração de `por` |
| Considerar que frações criam simultaneidade automaticamente | Frações subdividem duração, mas comandos sequenciais continuam sequenciais |
| Descartar toda forma de paralelismo por ser batch | Modelo finito permite eventos simultâneos sem exigir live coding |

## 5. Consequências

- A AST possui `ComandoGrupo`, mas a IR continua uma sequência de `SoundEvent` independente da sintaxe.
- A antiga invariante de voz única contígua não vale para todos os programas; a I6 atual considera início em zero, cobertura temporal sem lacunas e duração definida pelo maior fim.
- Nenhuma interface de áudio precisa receber a sintaxe `{ , }`: recebe apenas a Timeline compilada.
- A estrutura exige testes de simultaneidade, ordenação estável, cursor, aninhamento, volume, oitava e escopos.
- A decisão **não adiciona** live coding, mini-notação `[ ]` nem dot-chaining.

## 6. Rastreabilidade

CMP-07 #39 foi encerrada como concluída após o merge do PR #128. A gramática, o contrato do IR e a especificação estão nos documentos já atualizados por esse PR; este ADR preserva a motivação e o acordo de semântica, sem reescrever os arquivos da implementação.
