---
title: Especificação de Requisitos
layout: default
parent: Documentação
nav_order: 6
---

# Especificação de Requisitos (Ponto de Controle P1)

## 1. Objetivo do Interpretador
O projeto consiste no desenvolvimento de uma **Domain-Specific Language (DSL) focada na composição e execução de áudio**. O objetivo é permitir que o utilizador escreva instruções textuais (notas, ritmos, andamentos) num ficheiro de texto plano, que será lido em lote e validado pelo nosso compilador, gerando a reprodução sonora correspondente.

## 2. Arquitetura e Ferramentas
O projeto está sendo construído na linguagem base **C/C++** e utiliza as seguintes ferramentas de arquitetura de compiladores clássicas:
* **Flex:** Responsável pela Análise Léxica (reconhecimento dos tokens musicais, comandos, números e símbolos).
* **Bison:** Responsável pela Análise Sintática (validação das regras gramaticais e resolução de conflitos shift/reduce).
* **miniaudio:** Biblioteca C/C++ externa, responsável pelo motor de áudio. É ela que processa a reprodução dos sons após a validação completa do código-fonte.

## 3. Especificação da Linguagem (Sintaxe Base)
A linguagem musical foi desenhada de forma a ser o menos ambígua possível, utilizando vocabulário em português. As decisões de sintaxe e léxico consolidadas no parser são as seguintes:

* **Notas Musicais:** A linguagem reconhece a notação latina padrão (`do`, `re`, `mi`, `fa`, `sol`, `la`, `si`), incluindo sustenidos (`#`) e bemóis (`b`).
* **Oitavas:** Opcionais, mas quando usadas, devem estar coladas ao nome da nota (ex: `do4`). Se for omitida, o compilador adota a oitava corrente.
* **Separador de Duração:** Para evitar a ambiguidade matemática do hífen (como em `la4 - 2`), a linguagem exige obrigatoriamente a palavra-chave `por` para separar a ação da sua duração (ex: `toca do4 por tempo`).
* **Duração:** Medida em "tempos" (floats), compatíveis com o *Playhead* do motor de áudio.
* **Agrupamento de Blocos:** As estruturas compostas (como repetições) devem ser fechadas entre chaves `{ }`.
* **Comentários:** O interpretador ignora totalmente linhas iniciadas com `//` ou blocos contidos entre `/*` e `*/`.

**Exemplo Oficial de Instrução Válida:**
O bloco de código abaixo é a representação oficial das capacidades da linguagem, presente no ficheiro `exemplos/arpejo.mus` e totalmente aceito pela gramática atual.
```bash
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

## 4. Estruturas Lógicas e de Controlo
> *[EM CONSTRUÇÃO - Aguardando definição da equipe sobre que comandos exatos de variáveis (ex: andamento, tempo), ciclos de repetição e operações aritméticas estarão oficialmente suportados na versão P1]*

