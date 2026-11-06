---
title: "Ata — 07/10/2026"
layout: default
parent: Documentação
nav_order: 28
---

# Ata de Reunião — 07/10/2026

- **Projeto:** Interpretador de linguagem de programação musical
- **Disciplina:** Compiladores 1 — Turma 01 (Prof. Sergio) — FCTE/UnB
- **Horário:** 14:00 – 15:30
- **Local / formato:** Sala da FCTE / Presencial
- **Ata anterior:** [30/09/2026](./2026-09-30-ata-reuniao.md)

## Participantes

- Arthur Luiz
- Caio Melo Borges
- Cecília Costa
- Julia Patricio
- Marcella Anderle

## Pauta

1. Revisão do design e gerenciamento de memória da AST (DOC-03, CMP-04, CMP-08)
2. Progresso da divisão recursiva de arcos (MAT-05)
3. Mitigação de buffer underruns e estabilidade do áudio (DSP-04)
4. Modelagem formal de escopos estáticos para a linguagem
5. Análise da complexidade de features futuras (dot-chaining e closures)

## Discussões e decisões

### 1. Gerenciamento de memória e profundidade da AST (CMP-08, MAT-11)

Discutiu-se a prevenção de vazamentos de memória e falhas de segmentação:
- Em parsers gerados pelo Bison em C++, a recuperação de erros sintáticos pode descartar nós da pilha sem desalocar memória se não houver diretivas `%destructor`.
- **Decisão:** Estruturar a AST utilizando conceitos modernos de C++17 (`std::variant`, nós RAII encapsulados em `ast::Arvore`) e configurar `%destructor` no `parser.y` para liberar nós em caso de erro sintático.
- **Proteção contra estouro de pilha (CMP-08):** Definir um teto determinístico de 256 níveis de profundidade de aninhamento de nós na AST, rejeitando programas com recursão sintática patológica antes de esgotar a pilha do interpretador.

### 2. Estabilidade do pipeline de áudio (DSP-04)

Foram identificados riscos de perda de pacotes (*underrun*) quando a taxa de consumo de áudio supera a velocidade de preenchimento do buffer:
- Foi proposta a sinalização explícita de underrun na fila de eventos para monitorar a integridade acústica.
- Recomendou-se a introdução de um limitador suave (*soft-clipper*) no master bus de saída para impedir saturação digital quando múltiplas notas soam simultaneamente.

### 3. Modelagem de escopo estático

Ficou alinhado que o bloco `{ ... }` do `repita` delimita um escopo lexico local:
- Variáveis externas são lidas pelo bloco.
- Variáveis criadas dentro do bloco deixam de existir ao término da sua execução.
- A semântica deve acusar erro estático caso o usuário tente acessar uma variável fora de seu escopo de vida.

## Pendências / próximos passos

- [ ] Consolidar a documentação formal de arquitetura e decisões de design da AST (DOC-03)
- [ ] Configurar job de CI com AddressSanitizer (ASAN) para auditoria automatizada de vazamentos (MAT-11)
- [ ] Implementar a Tabela de Símbolos e o Analisador Semântico (CMP-13)
- [ ] Preparar os artefatos da Release M1 para homologação do marco
