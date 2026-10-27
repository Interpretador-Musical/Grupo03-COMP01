---
title: Notas da Release M1
layout: default
parent: Documentação
nav_order: 5
---

# Notas da Release M1 (Milestone 1 — Baseline Funcional)

**Data de Lançamento:** 27 de Outubro de 2026  
**Versão:** `v0.1.0-m1`

---

## Visão Geral da Release

A **Release M1** formaliza o primeiro marco histórico (*Milestone 1*) do **Compilador Musical (Grupo 03)**. O seu principal objetivo é **empacotar e homologar a primeira versão contínua funcional** originada das **Sprints 1 e 2**, servindo de base sólida e estável para a continuidade dos desenvolvimentos das **Sprints 3 e 4** (rumo ao Ponto de Controle 2).

Neste lançamento, o sistema comprova a viabilidade do compilador musical em lote (*batch*), entregando o fluxo *end-to-end* que traduz arquivos `.mus` em estruturas sintáticas e gera som reproduzível via `miniaudio`.

---

## O Que Esta Release Empacota (Sprints 1 e 2)

A Release M1 consolida o trabalho de fundação do compilador:

### 1. Entregas da Sprint 1: Elicitação, Descoberta e Lexer
- **Infraestrutura de Build:** Configuração moderna de compilação em C++17 via CMake integrando geradores **Flex** e **Bison**.
- **Modelagem de Domínio Musical:** Definição das estruturas fundamentais de eventos musicais e tempo (`Arc`, `Hap`, `Pattern`, `Pitch`, `SoundEvent`).
- **Prova de Conceito de Áudio:** Inicialização da biblioteca `miniaudio` e síntese de tom de teste senoidal de 440 Hz (`--test-tone`).
- **CLI & Entrypoint:** Estrutura unificada de linha de comando com controle de verbosidade e *logger* formatado.

### 2. Entregas da Sprint 2: Análise, Parser e Ponto de Controle 1 (P1)
- **Conexão End-to-End (`INT-03`):** Pipeline contínuo *String $\to$ AST $\to$ Timeline $\to$ Som*. O compilador lê o script `.mus`, constrói os nós da AST e gera a lista temporal de eventos de áudio.
- **Tratamento de Erros Léxicos e Sintáticos (`CMP-03`):** Mensagens de diagnóstico ricas com indicação de **linha e coluna** para tokens inválidos ou quebras de sintaxe.
- **Áudio Lock-Free (`SCH-03`, `DSP-03`):** Implementação de Ring Buffer SPSC (*Single-Producer Single-Consumer*) *lock-free*, transferindo eventos da timeline ao *callback* de áudio em tempo real sem bloqueios nem *buffer underruns*.
- **Inspeção Diagnóstica:** Flag `--tokens` na CLI para visualização tabular de todos os tokens emitidos pelo lexer.

---

## Critérios de Aceitação e Garantia de Qualidade (DoD — INT-04)

Como parte das atribuições da Engenharia de Release, todos os critérios de aceitação foram estritamente validados:

1. **Compilação Limpa (DoD 1):**
   - Compilação no WSL/Linux (GCC 15) com flags rigorosas `-Wall -Wextra -Werror` sem nenhum aviso (*zero warnings*).
   - Suíte de testes automatizados com **100% de sucesso no `ctest`**: 218 casos de teste e 21.722 asserções validadas pelo `doctest`.

2. **Executável Estático para Windows (DoD 2 e Requisito Não-Funcional):**
   - Binário `compilador.exe` gerado via compilação cruzada com MinGW-w64 (`-static -static-libgcc -static-libstdc++`).
   - Inspeção por `objdump` comprovando ligação estática completa: ausência de dependências externas de DLLs de C++ (`libstdc++`, `libgcc`, `libwinpthread`) ou bibliotecas externas de áudio. O executável vincula apenas DLLs padrão do sistema operacional Windows (`KERNEL32.dll` e `msvcrt.dll`).
   - Capacidade comprovada de receber arquivos de script `.mus` externos via linha de comando no terminal Windows/WSL e processá-los com sucesso.

---

## O Que Vem a Seguir (Sprints 3 e 4 — Rumo ao P2)

Com o fechamento da Release M1, as atenções se voltam para os ciclos em andamento e planejados:

- **Sprint 3 (Representação Visual, AST e Semântica):**
  - Conclusão da Análise Semântica e Tabela de Símbolos (`CMP-13`), garantindo tipagem forte e detecção de variáveis não declaradas.
  - Refatoração dos nós da AST para suportar expressões aninhadas (`CMP-04`).
  - Suporte gramatical a multiplicadores rítmicos (`CMP-05`) e início da mixagem polifônica (`DSP-05`).
  - Documentação da arquitetura formal da AST (`DOC-03`).

- **Sprint 4 (Verificação, Interpretação e Ponto de Controle 2):**
  - Expansão gramatical para estruturas de controle (condicionais `se/entao` e funções `defina/retorna`).
  - Encadeamento de métodos funcionais (*dot-chaining*).
  - Fechamento formal do material de submissão do Ponto de Controle 2 (P2).

---

## Como Executar a Release M1

No Windows (PowerShell / Prompt):
```powershell
# Execução direta com reprodução de áudio
.\compilador.exe exemplos\escala.mus

# Execução em lote sem áudio
.\compilador.exe --no-audio exemplos\escala.mus

# Inspecionar tabela de tokens
.\compilador.exe --tokens exemplos\escala.mus

# Ajuda completa
.\compilador.exe --help
```
