---
title: Cronograma
layout: default
parent: Documentação
nav_order: 3
---

# Cronograma

Tudo o que o grupo planeja para a construção do interpretador musical ao longo do semestre vive aqui. O cronograma reflete o fluxo de desenvolvimento do pipeline (Lexer → Parser → AST → Interpretador) aliado às entregas e documentação exigidas na disciplina.

## Resumo das Sprints

Registro cronológico dos ciclos de desenvolvimento — o período, os focos principais e os marcos de avaliação.

| Data / Período | Ciclo | Foco principal e Entregas |
|---|---|---|
| 10/08 a 26/08 | Sprint 1 | Elicitação, setup do ambiente (CMake/miniaudio) e análise léxica inicial no Flex. |
| 31/08 a 30/09 | Sprint 2 | Análise sintática (Bison), tratamento de erros de sintaxe, e Ponto de Controle 1 (P1). |
| 05/10 a 21/10 | Sprint 3 | Construção da Árvore Sintática (AST) e análise semântica (escopo e variáveis). |
| 26/10 a 11/11 | Sprint 4 | Interpretação *tree-walking*, agendamento de eventos na timeline e Ponto de Controle 2 (P2). |
| 16/11 a 25/11 | Sprint 5 | Otimizações, expansão rítmica na gramática e usabilidade final (Manual). |
| 30/11 a 09/12 | Sprint 6 | Testes integrados, Entrevistas Finais e encerramento do repositório. |

## Detalhamento de Tarefas e Issues

Documentação e tarefas técnicas previstas para cada ciclo, mapeadas diretamente para as issues no repositório do projeto:

### Sprint 1: Elicitação, Descoberta e Lexer
*   **CMP-01:** Configurar ambiente de build e esqueleto do Lexer/Parser
*   **MAT-01:** Definir estruturas de dados básicas (Arc, Hap e Pattern)
*   **SCH-01:** Criar Loop Principal e instanciar Thread de Tempo
*   **DSP-01:** Inicializar miniaudio e tocar áudio estático
*   **INT-01:** Estruturação do Logger CLI e Main Entrypoint
*   **CMP-02:** Parsing de sequências lineares simples
*   **MAT-02:** Mapeamento temporal de Ciclos (0.0 a 1.0)
*   **SCH-02:** Implementar Delta Time e relógio de alta precisão
*   **DSP-02:** Loader de arquivos WAV para Buffer na memória RAM
*   **INT-02:** Conectar entrada do terminal ao Lexer/Parser
*   **DOC-01:** Documentar Visão do Produto e Guia de Ambiente

### Sprint 2: Análise, Parser e P1
*   **CMP-03:** Tratamento de erros léxicos e sintáticos
*   **MAT-03:** Escrever Testes Unitários para intersecção de ciclos
*   **SCH-03:** Implementar Ring Buffer (Lock-Free) SPSC
*   **DSP-03:** Consumir Haps do Ring Buffer no Callback DSP
*   **INT-03:** Fluxo End-to-End conectado (String -> Som)
*   **DOC-02:** Redigir Especificação de Requisitos e Material do P1

### Sprint 3: Representação Visual, AST e Semântica
*   **CMP-04:** Refatorar nós da AST para suportar aninhamento futuro
*   **MAT-04:** Corrigir bugs de ponto flutuante (float epsilon) na junção de ciclos
*   **SCH-04:** Calibração fina do algoritmo de Lookahead
*   **DSP-04:** Mitigação absoluta de Buffer Underruns
*   **INT-04:** Validação e Lançamento da Release M1
*   **CMP-05:** Gramática para agrupamentos rítmicos (`[]`) e multiplicadores (`*`)
*   **MAT-05:** Algoritmo de divisão recursiva de Arcos
*   **SCH-05:** Otimização de Ring Buffer para Alta Densidade
*   **DSP-05:** Mixagem Polifônica Nativa (Soma Vetorial de PCM)
*   **INT-05:** Carregador Dinâmico de Dicionário de Samples
*   **DOC-03:** Documentar Arquitetura e Decisões de Design da AST

### Sprint 4: Verificação, Interpretação e P2
*   **CMP-06:** Adicionar dot-chaining de métodos funcionais (`.função()`)
*   **MAT-06:** Emular Closures e Functores via Ponteiros de Função
*   **SCH-06:** Propagação de Queries Lookahead pela Árvore Funcional
*   **DSP-06:** Pitch Shifting via Interpolação Linear (Playback Rate)
*   **INT-06:** Suíte Automatizada para Encadeamento Funcional
*   **CMP-07:** Gramática de Polirritmia Assimétrica (`{,}`)
*   **MAT-07:** Cobertura MC/DC nos Algoritmos de Intersecção de Arcos
*   **SCH-07:** Profiling CPU e Cache Optimization (Callgrind)
*   **DSP-07:** Algoritmo de Voice Stealing seguro
*   **INT-07:** Stress Test Simulando Darkpsy/Psycore (Alto BPM)
*   **DOC-04:** Registrar Estratégia de Testes e Material do P2

### Sprint 5: Organização e Refinamento
*   **CMP-08:** Tratamento de Edge Cases em Árvores Complexas
*   **MAT-08:** Fix de Drift Temporal Progressivo na Matemática do Ciclo
*   **SCH-08:** Varredura de Condições de Corrida Ocultas (Thread Sanitizer)
*   **DSP-08:** Otimização e Estabilização Final da Mixagem (Bug Bashing)
*   **INT-08:** Validação e Lançamento da Release M2 (Beta 0.5)
*   **CMP-09:** Parser Resiliente para Live Coding (ignorar falhas de input)
*   **MAT-09:** Estrutura de Ponteiros Atômicos (`AST_Active` e `AST_Next`)
*   **SCH-09:** Hot-swapping Thread-safe no Lookahead
*   **DSP-09:** Controle de Volume Master e Ganho Individual
*   **INT-09:** Interface CLI Interativa (Raw Mode)
*   **CMP-10:** Conectar Input Assíncrono ao Pipeline do Parser
*   **MAT-10:** Implementação de Arena Allocator (Memory Pool) para a AST
*   **SCH-10:** Quantização Matemática de Hot-Swapping (Swap no 1.0)
*   **DSP-10:** Soft-Clipper/Limiter no Master Bus
*   **INT-10:** Feedback Visual Sincronizado (Metrônomo CLI)
*   **DOC-05:** Criar Manual do Usuário e Padronizar Documentação de Código

### Sprint 6: Fechamento, Impacto e Entrevistas
*   **CMP-11:** Açúcar Sintático e Macros Rápidas no Lexer
*   **MAT-11:** Auditoria Definitiva de Memória (ASAN)
*   **SCH-11:** Hook para Restrição Real-Time Estrita (lock-free check)
*   **DSP-11:** Efeito DSP: Filtro Passa-Baixa (BiQuad LPF)
*   **INT-11:** Documentação, Makefiles Finais e Arquitetura
*   **CMP-12:** Code Review Final de Arquitetura e Warnings Cppcheck
*   **MAT-12:** Validação Final rítmica e matemática dos Arcos de Tempo
*   **SCH-12:** Auditoria Final de Concorrência e uso de CPU em Idle
*   **DSP-12:** Bateria Final de análise anti-falhas e cliques Sonoros
*   **INT-12:** Empacotamento, Performance Gravada e Release RC 1.0
*   **DOC-06:** Consolidar Relatório Final e Adequação do Repositório