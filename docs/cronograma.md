---
title: Cronograma
layout: default
parent: Documentação
nav_order: 2
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
*   **DOC-01:** Documentar Visão do Produto e Guia de Ambiente

### Sprint 2: Análise, Parser e P1
*   **CMP-02:** Parsing de sequências lineares simples
*   **MAT-02:** Mapeamento temporal de Ciclos (0.0 a 1.0)
*   **SCH-02:** Implementar Delta Time e relógio de alta precisão
*   **DSP-02:** Loader de arquivos WAV para Buffer na memória RAM
*   **INT-02:** Conectar entrada do terminal ao Lexer/Parser
*   **DOC-02:** Redigir Especificação de Requisitos e Material do P1

### Sprint 3: Representação Visual, AST e Semântica
*   **CMP-03:** Tratamento de erros léxicos e sintáticos
*   **MAT-03:** Escrever Testes Unitários para intersecção de ciclos
*   **SCH-03:** Implementar Ring Buffer (Lock-Free) SPSC
*   **DSP-03:** Consumir Haps do Ring Buffer no Callback DSP
*   **INT-03:** Fluxo End-to-End conectado (String -> Som)
*   **DOC-03:** Documentar Arquitetura e Decisões de Design da AST

### Sprint 4: Verificação, Interpretação e P2
*   **CMP-04:** Refatorar nós da AST para suportar aninhamento futuro
*   **MAT-04:** Corrigir bugs de ponto flutuante (float epsilon) na junção de ciclos
*   **SCH-04:** Calibração fina do algoritmo de Lookahead
*   **DSP-04:** Mitigação absoluta de Buffer Underruns
*   **INT-04:** Validação e Lançamento da Release M1
*   **DOC-04:** Registrar Estratégia de Testes e Material do P2

### Sprint 5: Organização e Refinamento
*   **CMP-05:** Gramática para agrupamentos rítmicos (`[]`) e multiplicadores (`*`)
*   **DOC-05:** Criar Manual do Usuário e Padronizar Documentação de Código

### Sprint 6: Fechamento, Impacto e Entrevistas
*   **DOC-06:** Consolidar Relatório Final e Adequação do Repositório
*   **INT-05:** Realizar testes integrados e validação final da sintaxe e áudio para a apresentação (Entrevistas finais)