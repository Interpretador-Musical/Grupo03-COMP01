# SCH-11 — Auditoria das restrições Real-Time

## 1. Objetivo

A SCH-11 (#61) implementa uma auditoria estática para identificar operações potencialmente bloqueantes ou incompatíveis com a execução em tempo real no caminho crítico de processamento de áudio.

O verificador está localizado em `scripts/check_realtime.py` e utiliza Python 3, sem dependências externas.

## 2. Cobertura

A auditoria verifica 15 funções distribuídas entre os seguintes componentes:

| Componente | Funções auditadas |
|---|---|
| `audio_engine.cpp` | `dataCallback`, `render`, `renderSine`, `renderRealTime`, `allocateVoice`, `isBetterToSteal` |
| `timeline_player.cpp` | `TimelinePlayer::render` |
| `audio_buffer.cpp` | `AudioBuffer::sampleLinear` |
| `sample_bank.cpp` | `SampleBank::get` |
| `ring_buffer.h` | `pop` |
| `limiter.h` | `softClip`, `softClipBuffer` |
| `wavetable.h` | `sineTable`, `lookupSine`, `frequencyToPhaseIncrement` |

O verificador também rejeita arquivos ausentes e funções que deixaram de ser encontradas.

## 3. Operações verificadas

O script identifica padrões associados a:

- Alocação e desalocação dinâmica, como `malloc`, `calloc`, `realloc`, `free`, `new` e `delete`.
- Operações de entrada e saída, incluindo `printf`, `fprintf`, funções de arquivos e streams como `std::cout`.
- Sincronização potencialmente bloqueante, incluindo mutexes, locks, condition variables e operações de espera.
- Chamadas de suspensão, como `sleep`, `usleep` e `sleep_for`.
- Inicializações estáticas locais por lambda ou por chamada de função.

Comentários e literais de texto são mascarados antes da análise, reduzindo falsos positivos.

## 4. Execução

Para executar somente a auditoria:

```bash
python3 -B scripts/check_realtime.py
```

Para executar os testes de regressão:

```bash
python3 -B -m unittest discover \
  -s scripts \
  -p 'test_check_realtime.py' \
  -v
```

Para executar os testes integrados ao CTest:

```bash
ctest --test-dir build \
  -R '^sch11_' \
  --output-on-failure
```

## 5. Integração contínua

Dois testes foram registrados em `tests/CMakeLists.txt`:

- `sch11_auditoria_realtime`: executa o verificador sobre o código-fonte.
- `sch11_testes_verificador`: executa os testes unitários do analisador.

O workflow principal de CI utiliza CMake e executa o CTest, incluindo esses testes.

## 6. Inicialização da wavetable

Durante a auditoria, foi identificada uma inicialização estática local em `sineTable()`, anteriormente executada na primeira chamada à função.

A implementação foi alterada para utilizar uma variável `inline const` em escopo de namespace. O construtor de `AudioEngine` também acessa a tabela antecipadamente, antes da inicialização da reprodução.

A tabela continua utilizando inicialização dinâmica em C++17. A solução retira a inicialização estática local do corpo de `sineTable()` e prepara a tabela antes da operação de áudio associada ao motor.

## 7. Limitações

A auditoria é baseada em expressões regulares e extração textual de funções. Portanto:

- Não realiza análise semântica completa de C++.
- Não constrói automaticamente todo o grafo de chamadas.
- Pode deixar de detectar operações bloqueantes executadas indiretamente por funções não auditadas.
- Não identifica todas as formas de inicialização dinâmica.
- Não substitui ferramentas de análise dinâmica, testes de concorrência ou medições de latência.
- Não oferece uma prova formal de execução lock-free ou wait-free.
- Pode produzir falsos positivos ou falsos negativos quando o código utiliza macros ou sintaxe C++ não contemplada pelo analisador.

A lista de funções auditadas deve ser revisada sempre que o caminho de processamento de áudio for modificado.

## 8. Validação

Na validação local da SCH-11:

- 15 funções críticas foram auditadas.
- 17 testes de regressão do verificador foram aprovados.
- 35 testes registrados no CTest foram aprovados.
- O projeto compilou com sucesso.

Esses resultados demonstram que os padrões cobertos pela auditoria foram verificados e que os testes existentes não identificaram regressões. Eles não garantem ausência absoluta de bloqueios ou cumprimento de limites temporais em todas as condições de execução.

## 9. Referências internas

- Issue SCH-11: `#61`.
- Script: `scripts/check_realtime.py`.
- Testes: `scripts/test_check_realtime.py`.
- Integração: `tests/CMakeLists.txt`.
- Implementação da wavetable: `src/audio/wavetable.h`.
