---
title: Diretrizes de Escopo e Prioridades
layout: default
parent: Documentação
nav_order: 11
---

# Diretrizes de escopo — Compilador Musical Batch

**Referência:** deliberações arquiteturais de 16/09 e 02/11/2026; situação do repositório verificada após a integração do PR #128. **Natureza:** complemento operacional ao [ADR 001]({{ site.baseurl }}{% link decisao_modelo_batch.md %}), sem substituir o histórico do ADR ou reabrir decisões já executadas.

## 1. Princípio permanente

O produto é um **compilador de músicas finitas**. Um arquivo `.mus` é analisado integralmente antes de ser entregue à engine de áudio. As etapas demonstráveis são leitura, Flex, Bison, AST, análise semântica, geração da Timeline como IR, emissão determinística da IR e execução/reprodução. A etapa de geração da **saída-alvo final** é uma entrega distinta, identificada como COMP-C #108.

O projeto seguirá o caminho mais simples que satisfaça o escopo acadêmico e que permita demonstração reproduzível. Não serão introduzidos requisitos de TidalCycles (padrões infinitos, hot-swap, lookahead funcional ou live coding) nem funcionalidades especulativas que comprometam testes ou prazo.

## 2. Polirritmia finita: decisão executada

A proposta discutida para a CMP-07 usava a denominação **polirritmia ou polifonia de bloco**, não polimetria contínua. Em 02/11, o grupo definiu o contrato e o PR #128 o implementou; ele já foi integrado à `main`.

| Pergunta | Decisão vigente |
|---|---|
| Quando começa cada trilha? | Todas partem do mesmo instante de entrada do grupo. |
| Quanto dura o grupo? | Até o maior instante final das trilhas. |
| Uma trilha curta espera? | Sim: a música seguinte só inicia após o término da mais longa. |
| `por` sofre reescala? | **Não**: expressa duração absoluta em beats em qualquer contexto. |
| O cursor seguinte é qual? | O do fim da trilha mais longa. |
| Como se comportam oitava e volume? | Alterações internas não escapam do grupo. |
| `andamento` pode mudar no grupo? | Não: erro semântico; o pulso compartilhado é preservado. |
| Variáveis de fora podem ser alteradas no grupo? | Não: reatribuição externa é recusada pela análise semântica. |
| Como o grupo aparece na IR? | Como eventos temporizados simultâneos, sem nó de grupo na Timeline. |
| A sintaxe obriga live coding? | Não: continua um programa finito, compilado antes de tocar. |

Exemplo integrado:

```mus
andamento 120
{
    repita 3 vezes { toca do4 por 1 / 3 },
    repita 4 vezes { toca sol3 por 1 / 4 }
}
toca do5 por 1
```

As duas trilhas duram um beat. A nota após o grupo começa depois desse beat, a 0,5 segundo quando o andamento é 120 BPM.

**Distinção necessária:** `por 1 / 3` e `por 1 / 4` isoladamente representam subdivisões. A simultaneidade vem da sintaxe `{ trilha1, trilha2 }`. Não atribuir às frações o papel do paralelismo.

A especificação formal vigente está em [Gramática EBNF]({{ site.baseurl }}{% link gramatica_ebnf.md %}); o contrato temporal está em [Representação Intermediária]({{ site.baseurl }}{% link representacao_intermediaria.md %}). O [ADR 002]({{ site.baseurl }}{% link adr_002_trilhas_paralelas.md %}) reúne a motivação e as consequências sem alterar os arquivos já entregues no PR #128.

## 3. Decisão de entrega P2 e trabalho futuro

O núcleo do P2 em 04/11 é a linguagem e a cadeia batch já integradas: Flex → Bison → AST → análise semântica → Timeline verificável. A existência da CMP-07 integrada é evidência adicional, mas não se deve tornar closures, funções, filtros ou otimizações novas requisitos retroativos para concluir o P2.

**CMP-06 (dot-chaining).** Adiado: o exemplo da issue encadeia métodos sobre listas `[ ]`, que também ficaram fora do escopo (CMP-05), e implementar exigiria token, gramática, nós de AST, tipo lista e um conjunto de métodos. Quando retomado, a cadeia deve ser um único nó achatado (`base` + vetor de chamadas), com profundidade constante, para não bater no limite de 256 níveis da CMP-08. `tests/test_encadeamento.cpp` guarda que a sintaxe hoje é recusada com erro limpo.

As prioridades são:

1. **Sprint 04/P2:** corrigir inconsistências nos registros, conferir evidências, validar build/CTest, garantir que a submissão acadêmica seja verificada separadamente do merge do relatório.
2. **Sprint 05:** escolher um único formato-alvo musical e implementar COMP-C #108; resolver os defeitos #123, #124 e #125 e as falhas de áudio reproduzidas.
3. **Sprint 06:** consolidar INT-11 #63 e DOC06 #38, realizar testes finais e preparar INT-12 #68.

Para manter o escopo simples, a orientação desta consolidação é encerrar como `not planned` CMP-05 #27 (mini-notação), CMP-06 #33 (dot-chaining), INT-05 #31 (samples nomeados), SCH-07 #41 (profiling especulativo), DSP-09 #52 (controles adicionais sem defeito verificado), MAT-10 #55 (Arena Allocator), CMP-11 #59 (macros) e DSP-11 #62 (BiQuad LPF). Não confundir essa recomendação documental com alteração já aplicada no GitHub; nenhum estado será modificado automaticamente.

O PR #129 (MAT-06 #34) foi integrado à `main` e acrescentou a infraestrutura de ambientes léxicos e closures. Essa implementação não significa que a DSL já suporte as construções `defina` e `retorna`, que continuam sem integração sintática. A infraestrutura deve permanecer compatível com os grupos de trilhas paralelas da CMP-07 (#39), já integrados pelo PR #128. Não serão introduzidas novas funcionalidades de linguagem apenas para ampliar o escopo da MAT-06.

## 4. Regras para documentação e acompanhamento

- Um PR aberto não configura conclusão; um PR integrado pode ser rastreado como concluído.
- Registros históricos (ADR 001 e relatório P2) conservam a data da evidência. Correções posteriores devem indicar claramente o novo contexto.
- A DOC-A #106 e a DOC-B #107 já foram entregues. Não devem depender retroativamente da INT-11 #63, prevista para a documentação final.
- A DOC-04 #26 (relatório P2) foi integrada por PR #121; confirmar a entrega formal do formulário como obrigação distinta.
- Números de testes devem estar associados a commit ou execução; não atualizar indiscriminadamente resultados históricos.
- É incorreto chamar a Timeline de 3AC clássico ou de saída pós-otimização: é uma IR musical de eventos temporizados.
- Um ADR posterior complementa decisões executadas sem apagar deliberações anteriores.

## 5. Fontes internas

- [ADR 001]({{ site.baseurl }}{% link decisao_modelo_batch.md %}) — decisão batch.
- [ADR 002]({{ site.baseurl }}{% link adr_002_trilhas_paralelas.md %}) — contrato de grupos paralelos.
- [Relatório P2]({{ site.baseurl }}{% link relatorio_p2.md %}) — fotografia das evidências do PR #121.
- [Cronograma]({{ site.baseurl }}{% link cronograma.md %}) — marcos e sprints.
