---
title: "Ata — 09/09/2026"
layout: default
parent: Documentação
nav_order: 24
---

# Ata de Reunião — 09/09/2026

- **Projeto:** Interpretador de linguagem de programação musical
- **Disciplina:** Compiladores 1 — Turma 01 (Prof. Sergio) — FCTE/UnB
- **Horário:** 14:00 – 15:30
- **Local / formato:** Sala da FCTE / Presencial
- **Ata anterior:** [02/09/2026](./2026-09-02-ata-reuniao.md)

## Participantes

- Arthur Luiz
- Caio Melo Borges
- Cecília Costa
- Julia Oliveira
- Marcella Anderle

## Pauta

1. Revisão do progresso das frentes DSP, MAT, SCH e INT
2. Validação da primeira emissão de som pelo dispositivo (DSP-01)
3. Modelagem do Playhead e conversão de BPM para segundos (MAT-02)
4. Sincronismo do loop de tempo com relógio monotônico (SCH-02)
5. Definição da interface de linha de comando (CLI) e tratamento de opções (INT-01)

## Discussões e decisões

### 1. Primeira emissão sonora via miniaudio (DSP-01)

Foi demonstrada a primeira reprodução de som estático gerado proceduralmente:
- A biblioteca `miniaudio` foi integrada no projeto via header único em `include/miniaudio.h`.
- Foi adicionada a flag `--test-tone` na CLI para sintetizar uma onda senoidal pura em 440 Hz (Lá central) por 3 segundos.
- O teste valida que a camada de comunicação com os drivers de áudio do sistema operacional (ALSA/PulseAudio/CoreAudio) está operacional.

### 2. O Playhead e mapeamento temporal (MAT-01, MAT-02)

O grupo alinhou como o conceito de *playhead* (cursor de tempo) atua matematicamente:
- A unidade musical básica no código-fonte é o "tempo" relativo (ex: `1.0` para semínima, `0.5` para colcheia).
- O comando `andamento <BPM>` define a taxa de conversão para segundos reais:
  $$\text{segundos} = \text{tempo} \times \left(\frac{60}{\text{BPM}}\right)$$
- O `Playhead` mantém o tempo acumulado, avançando estritamente após cada comando de nota ou pausa.

### 3. Loop do Scheduler com relógio monotônico (SCH-01, SCH-02)

Discutiu-se o risco de desvio temporal (*drift*) em execuções longas:
- **Decisão:** utilizar estritamente `std::chrono::steady_clock` no motor de agendamento em vez de `system_clock`, prevenindo que ajustes no relógio do sistema afetem a reprodução sonora.
- A thread de tempo calculará o $\Delta t$ (*delta time*) a cada iteração do loop para garantir precisão e estabilidade.

### 4. Padronização da CLI e logs (INT-01)

A CLI aceitará flags padronizadas (`--help`, `--version`, `--test-tone`, `--demo`, `--bpm`).
- Mensagens de log de depuração respeitarão níveis de severidade (`DEBUG`, `INFO`, `WARN`, `ERROR`), com saída formatada para o terminal.

## Pendências / próximos passos

- [ ] Estruturar a infraestrutura de testes unitários com framework leve (doctest + CTest)
- [ ] Implementar a gramática inicial do Flex (`lexer.l`) e do Bison (`parser.y`) (CMP-02)
- [ ] Desenvolver o decodificador de arquivos de áudio WAV para suporte a amostras (DSP-02)
- [ ] Proteger o compartilhamento de eventos entre threads com fila segura
