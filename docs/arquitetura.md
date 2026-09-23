---
title: Visão de Arquitetura
layout: default
parent: Documentação
nav_order: 4
---

# Visão do Produto e Arquitetura

O Interpretador Musical (Grupo 03) atua fundamentalmente como um **compilador musical** de uma *Domain-Specific Language* (DSL)[cite: 24, 27]. A arquitetura do projeto opera sob uma estrita separação de responsabilidades: o núcleo do compilador (lógica de linguagem) e o motor de áudio (geração de sinal sonoro)[cite: 24].

## 1. O Compilador (Pré-processamento)

O núcleo do compilador, desenvolvido em C/C++ com Flex e Bison, não emite som diretamente nem executa instruções em tempo real[cite: 24, 25, 27]. O fluxo de funcionamento ocorre em lote (*batch*):
1. **Leitura:** O compilador recebe o código-fonte por inteiro (arquivo `.mus`).
2. **Análise Léxica e Sintática:** O Flex e o Bison validam as regras da linguagem e agrupam os tokens musicais (comandos, notas, durações)[cite: 24, 27].
3. **Construção da Árvore (AST):** Transforma o código numa Árvore Sintática Abstrata[cite: 24, 27].
4. **Geração de Eventos:** Percorre a AST e compila o código para uma **lista estática de eventos musicais temporizados** na memória[cite: 24]. 

Cada instrução válida gera um objeto `SoundEvent` contendo[cite: 24]:
* **`startTime`**: O momento em que o som deve iniciar na linha temporal.
* **`duration`**: O tempo total de sustentação da nota.
* **`frequency`**: A frequência (Hz) da nota musical.
* **`volume`**: A intensidade/ganho do som.

## 2. O Motor de Áudio (Ação de Play)

Apenas após a compilação completa do ficheiro, o sistema aguarda um gatilho de execução ("Play"). Ao ser acionado, a lista completa de eventos é enviada para o motor de áudio externo — a biblioteca **miniaudio** (em C/C++)[cite: 24, 25].

É a *miniaudio* que dialoga com a interface de áudio do sistema operativo para sintetizar as frequências e emitir a música nos altifalantes[cite: 24, 25]. Isto garante que o núcleo do compilador se foque na estrutura da linguagem, delegando a complexidade do processamento de sinal para o motor de áudio.
