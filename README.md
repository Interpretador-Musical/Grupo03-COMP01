# 🎵 Compilador Musical — Grupo 03

![C++17](https://img.shields.io/badge/C++-17-blue.svg)
![Build](https://img.shields.io/badge/build-CMake-brightgreen)
![Testes](https://img.shields.io/badge/testes-41%20CTest%20%7C%20doctest-success)
![CI](https://img.shields.io/badge/CI-ASAN%20%7C%20TSAN-blueviolet)
![Status](https://img.shields.io/badge/status-Sprint%2006%20(Batch)-blue)
![Licença](https://img.shields.io/badge/licen%C3%A7a-MIT-green.svg)

Uma **Domain-Specific Language (DSL)** criada para transformar código-fonte em música. O programa escrito nesta linguagem não imprime texto: ele é analisado, validado, traduzido para uma representação intermediária formal (`Timeline`) e compilado para áudio ao vivo ou arquivo WAV PCM 16-bit offline. Todo o pipeline — analisador léxico (Flex), sintático (Bison + Arena AST), semântico (Tabela de Símbolos), otimização (dobra de constantes), geração de IR e gerador WAV — foi construído para compilação em lote (*batch*).

Trabalho prático da disciplina de **Compiladores 1** (Turma 01, Prof. Sérgio Freitas) — FCTE/UnB.

📄 **Site Oficial e Documentação:** [https://interpretador-musical.github.io/Grupo03-COMP01/](https://interpretador-musical.github.io/Grupo03-COMP01/)  
📑 **Relatório Final (Sprint 06):** [Relatório Final do Projeto](https://interpretador-musical.github.io/Grupo03-COMP01/relatorio_final.html)  
🚀 **Release Final e Demonstração (INT-12):** [Release Final e Roteiro de Demonstração](https://interpretador-musical.github.io/Grupo03-COMP01/release_final.html)  
📊 **Relatório de Verificação P2:** [Relatório e Evidências do P2](https://interpretador-musical.github.io/Grupo03-COMP01/relatorio_p2.html)  
📖 **Manual do Usuário (`.mus`):** [Manual do Usuário](https://interpretador-musical.github.io/Grupo03-COMP01/manual_usuario.html)  
🏗️ **Arquitetura Completa:** [Visão de Arquitetura](https://interpretador-musical.github.io/Grupo03-COMP01/arquitetura.html)  
🔊 **Saída-Alvo (WAV):** [Saída-Alvo Musical (WAV)](https://interpretador-musical.github.io/Grupo03-COMP01/saida_alvo.html)  
📋 **Governança e Processo:** [Governança e Práticas da Equipe](https://interpretador-musical.github.io/Grupo03-COMP01/governanca.html)

---

## 🚀 Guia de Instalação Rápida

O compilador é escrito em **C++17** e depende das ferramentas clássicas de engenharia de compiladores (GNU Flex e GNU Bison). O processamento de sinais digitais (DSP) utiliza a biblioteca `miniaudio` (versionada no repositório em `include/miniaudio.h`).

### 1. Dependências do Sistema

- Compilador C++ com suporte a C++17 (`g++` 9+ ou `clang` 10+)
- **CMake** (versão 3.16 ou superior)
- **GNU Flex** (Analisador Léxico)
- **GNU Bison** (Analisador Sintático)
- Bibliotecas de desenvolvimento de áudio do sistema operacional

**Linux (Ubuntu/Debian) e Windows (via WSL — Recomendado):**
```bash
sudo apt update
sudo apt install build-essential cmake flex bison libasound2-dev libpulse-dev -y
```

**macOS (Xcode Command Line Tools + Homebrew):**
O `parser/parser.y` é compatível com o Bison 2.3 e o Flex incluídos no Xcode Command Line Tools (mesmo ambiente do CI de macOS), bastando instalar o CMake via Homebrew:
```bash
xcode-select --install
brew install cmake
```

### 2. Clonando e Compilando

Clone o repositório e faça a compilação via CMake:

```bash
git clone https://github.com/Interpretador-Musical/Grupo03-COMP01.git
cd Grupo03-COMP01

# Configura o build out-of-source e compila
cmake -S . -B build
cmake --build build -j
```

---

## 💻 Como Usar

O binário executável `compilador` é gerado na pasta `build/`.

```bash
# Ajuda e informações gerais
./build/compilador --help                   # Exibe comandos e opções disponíveis
./build/compilador --version                # Exibe a versão do compilador

# Validação e depuração de hardware de áudio
./build/compilador --test-tone              # Toca senoide de teste de 440 Hz por 3s
./build/compilador --demo                   # Imprime uma timeline de demonstração
./build/compilador --loop                   # Executa o motor de tempo contínuo (Ctrl+C encerra)

# Etapas formais de compilação (Fase 2 valida a AST em Arena automaticamente antes da Fase 3)
./build/compilador --tokens programa.mus                          # [Fase 1] Tabela de tokens (Flex)
./build/compilador --simbolos programa.mus                        # [Fase 3] Tabela de Símbolos e tipos (Semântica)
./build/compilador --sem-otimizacao --emit-timeline programa.mus  # [Fase 4 -> 5] Desativa dobra de constantes (COMP-D) ao emitir IR
./build/compilador --emit-timeline programa.mus                   # [Fase 5] Emite a Timeline IR em CSV (sem áudio)
./build/compilador -o saida.wav programa.mus                      # [Fase 6] Gera a saída-alvo WAV PCM 16-bit offline (COMP-C)

# Execução e reprodução sonora
./build/compilador --no-audio programa.mus   # Compila e valida integralmente sem tocar som
./build/compilador programa.mus              # Pipeline completo: compila, valida e toca o arquivo
./build/compilador --volume 0.5 programa.mus # Ajusta o Volume Master final em [0.0, 1.0] (ao vivo ou com -o)
./build/compilador --lpf 800 programa.mus    # Aplica filtro passa-baixa Master em [20, 20000] Hz (ao vivo ou com -o)
```

---

## ⚙️ Arquitetura e Pipeline do Compilador

O projeto opera estritamente sob o **modelo batch de programa finito** ([ADR 001](https://interpretador-musical.github.io/Grupo03-COMP01/decisao_modelo_batch.html)). O arquivo `.mus` de entrada é completamente analisado, validado, otimizado e traduzido para uma **Representação Intermediária (Timeline)** antes da geração do arquivo WAV final ou da emissão sonora ao vivo.

```text
Código-fonte (.mus)
       │
       ▼
 1. Flex (Lexer)             → Varredura léxica, figuras rítmicas e extração de tokens
       │
       ▼
 2. Bison (Parser)           → Validação LALR(1) e AST tipada em Arena (MAT-10, teto 256)
       │
       ▼
 3. Análise Semântica        → Tabela de Símbolos, escopos estáticos e checagem de tipos
       │
       ▼
 4. Dobra de Constantes      → Avaliação em tempo de compilação (COMP-D, --sem-otimizacao)
       │
       ▼
 5. Interpretador (IR)       → Geração da Timeline linear de SoundEvents (6 invariantes)
       │
       ├──▶ --emit-timeline      → Inspeção da IR em CSV determinístico (COMP-B)
       ├──▶ 6a. -o / --saida     → Saída-alvo WAV PCM 16-bit offline, 44,1 kHz mono (COMP-C)
       └──▶ 6b. Fila SPSC + AudioEngine → Wavetable (DSP-08) + BiQuad LPF (DSP-11) + Limitadores + Volume Master → Som
```

As Fases 1 a 4 são encadeadas pela fachada `src/frontend/frontend.{h,cpp}` (`CMP-12`) e pelos módulos `src/semantic/` e `src/otimizacao/` antes da geração da IR:

1. **Análise Léxica (`Flex`):** Rastreia linha e coluna de cada lexema (`YY_USER_ACTION`), expande figuras rítmicas (`semibreve` a `semifusa`) e `vez` como açúcar sintático (`CMP-11`), e repassa tokens desconhecidos ao parser.
2. **Análise Sintática (`Bison`):** Gramática livre de contexto LALR(1) sem conflitos (`%expect 0`). Os nós da AST vivem numa **Arena de memória** (`src/interpreter/arena.{h,cpp}`, `MAT-10`) vinculada à `ast::Arvore`, com teto determinístico de **256 níveis de profundidade** (`CMP-08`).
3. **Análise Semântica:** Infere tipos (`numero`, `nota`, `booleano`, `string`), resolve escopos locais de `repita` e trilhas paralelas, impede redeclarações inválidas e detecta variáveis não declaradas (`CMP-13`).
4. **Otimização (`COMP-D`):** Passagem opcional (ligada por padrão, desligada com `--sem-otimizacao`) de dobra de constantes sobre a AST antes da geração da IR, garantindo saída bit a bit idêntica.
5. **Representação Intermediária (IR):** `Timeline` ordenada de `SoundEvent`s governada por 6 invariantes formais (`COMP-A`) e exportável em CSV determinístico (`--emit-timeline`, `COMP-B`).
6. **Saída-Alvo e Execução em Áudio:** Geração offline em **WAV PCM 16-bit** (`-o` / `--saida`, `COMP-C`), reproduzível em qualquer player sem repetir o parsing, ou execução ao vivo via ring buffer lock-free SPSC compartilhando a mesma cadeia DSP: síntese por wavetable (`DSP-08`), samples WAV (`INT-05`), envelopes adaptativos de até 5 ms (#123), filtro passa-baixa BiQuad RBJ por voz e no master bus (`DSP-11`), *voice stealing* (`DSP-07`), ganhos por subgrupo, limitador de pico + *soft-clipper* (`BUG-AUDIO` #109, `DSP-10`) e Volume Master suavizado (`DSP-09`).

### Exemplo de Entrada e Saída Compilada

Usando `./build/compilador` sobre `exemplos/arpejo.mus`:

```bash
# [Fase 5] Emite a Representação Intermediária (14 eventos em 4 s a 120 BPM)
./build/compilador -q --emit-timeline exemplos/arpejo.mus
# indice,inicio_s,duracao_s,frequencia_hz,volume
# 1,0.000000,0.250000,261.625565,0.800000
# ...
# 14,3.500000,0.500000,0.000000,0.000000

# [Fase 6] Gera a saída-alvo WAV reproduzível sem repetir o parsing
./build/compilador -o arpejo.wav exemplos/arpejo.mus
# [INFO ] saída WAV gerada: arpejo.wav (176400 frames, 4 s, 352844 bytes)
```

---

## 🧪 Testes Automatizados e Qualidade

O projeto conta com suíte de testes unitários (`doctest`) e de integração/CLI (`CTest`) executados em cada commit via GitHub Actions (Ubuntu, macOS, Cppcheck, ASan/UBSan e TSan):

```bash
# Executa todos os testes pelo CTest (41 testes automatizados)
ctest --test-dir build --output-on-failure

# Ou executa diretamente o binário da suíte doctest (524 casos na suíte padrão; 537 em mus_tests --count)
./build/tests/mus_tests
```

### Auditoria com Sanitizers

Para verificar a ausência de vazamentos de memória e integridade concorrente na suíte de testes:

```bash
# Opção 1: Via opções do CMake (ENABLE_ASAN e ENABLE_TSAN)
# Build com AddressSanitizer (ASAN e UBSan)
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug -DENABLE_ASAN=ON
cmake --build build-asan -j
ctest --test-dir build-asan --output-on-failure

# Build com ThreadSanitizer (TSAN)
cmake -S . -B build-tsan -DCMAKE_BUILD_TYPE=Debug -DENABLE_TSAN=ON
cmake --build build-tsan --target mus_tests -j
./build-tsan/tests/mus_tests

# Opção 2: Passando flags diretamente (idêntico à CI no GitHub Actions)
# ASAN + UBSan:
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer -g -O1" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"
cmake --build build-asan -j
ctest --test-dir build-asan --output-on-failure

# TSAN:
cmake -S . -B build-tsan -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="-fsanitize=thread -g -O1" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=thread"
cmake --build build-tsan --target mus_tests -j
TSAN_OPTIONS=halt_on_error=1 ./build-tsan/tests/mus_tests
```

---

## 🎹 A Linguagem (Sintaxe e Capacidades)

A sintaxe adota vocabulário em português estruturado para eliminar ambiguidades:

- **Notas:** Padrão latino (`do`, `re`, `mi`, `fa`, `sol`, `la`, `si`), sustenidos (`#`) e bemóis (`b`).
- **Oitavas:** Anexadas à nota (ex: `do4`, `la4`) ou configuradas globalmente (`oitava 4`).
- **Separador Obrigatório `por`:** Isola a altura da duração, permitindo expressões aritméticas sem ambiguidades (ex.: `toca la4 - 2 por tempo * 2`).
- **Figuras Rítmicas e `vez`:** Durações como `semibreve`, `minima`/`mínima`, `seminima`/`semínima`, `colcheia`, `semicolcheia`, `fusa`, `semifusa`, e `repita 1 vez` (`CMP-11`).
- **Trilhas Paralelas (`{ , }`):** Permitem vozes simultâneas, acordes e polirritmias ([ADR 002](https://interpretador-musical.github.io/Grupo03-COMP01/adr_002_trilhas_paralelas.html)).
- **Samples WAV:** `toca "PERC_09" por colcheia` reproduz arquivos `.wav` da pasta `samples/` (`INT-05`).
- **Controle de Estado:** `andamento <BPM>`, `oitava <n>`, `volume <vol>`, `filtro <Hz>` (passa-baixa BiQuad RBJ por voz, `DSP-11`).

Exemplo literal de programa válido (`exemplos/arpejo.mus`, cuja saída é exibida nos exemplos acima):

```text
// Arpejo de Dó maior — quatro compassos.
//
// O playhead avança a cada nota, então o mesmo trecho repetido quatro vezes
// vira quatro compassos de música, e não quatro notas simultâneas.

andamento 120
volume 0.8

tempo = 0.5        // duração em tempos: 0.5 é uma colcheia

repita 4 vezes {
    toca do4  por tempo
    toca mi4  por tempo
    toca sol4 por tempo
}

/* Fecha na nota sol4, com o dobro da duração.
   A altura é uma expressão: lá 4 menos 2 semitons dá sol 4. */
toca la4 - 2 por tempo * 2

pausa por 1.0
```

---

## 📂 Estrutura do Repositório

```text
├── .github/workflows/    # Workflows de CI (build, Cppcheck, CTest, ASAN, TSAN e release)
├── src/                  # Código-fonte principal em C++17
│   ├── main.cpp          # Entrypoint da CLI e controle das fases de compilação
│   ├── audio/            # Motor miniaudio, wavetable, BiquadFilter, AudioBuffer, SampleBank, limitadores e GainSmoother
│   ├── cli/              # Parsing de argumentos da linha de comando e logger
│   ├── codegen/          # Gerador da saída-alvo musical em WAV PCM 16-bit (COMP-C)
│   ├── core/             # Estruturas de dados (SoundEvent, Playhead, Timeline, RingBuffer)
│   ├── engine/           # Loop de tempo monotônico e scheduler
│   ├── frontend/         # Invólucro do Scanner Flex e Parser Bison
│   ├── interpreter/      # AST, Arena Allocator (MAT-10), ambiente/closures e interpretador
│   ├── otimizacao/       # Dobra de constantes sobre a AST (COMP-D)
│   └── semantic/         # Analisador semântico, tabela de símbolos e checagem de tipos
├── lexer/                # Analisador léxico Flex (lexer.l)
├── parser/               # Analisador sintático Bison (parser.y)
├── include/              # Dependências single-header (miniaudio.h, doctest.h)
├── scripts/              # Validação de release (validar_release.sh) e auditoria Real-Time (check_realtime.py)
├── tests/                # Suíte completa de testes unitários, espectrais, integração e estresse
├── samples/              # Amostras WAV decodificadas pelo SampleBank
├── exemplos/             # Scripts musicais (.mus) de demonstração
└── docs/                 # Site do projeto (GitHub Pages), manual, especificações, ADRs e atas
```

---

## 👥 Integrantes e Rotatividade

O projeto utiliza práticas ágeis com rotatividade de papéis a cada Sprint:

- **Arthur Luiz** ([@ArthurLuizUnB](https://github.com/ArthurLuizUnB))
- **Caio Melo Borges** ([@CaioMelo25](https://github.com/CaioMelo25))
- **Cecília Costa** ([@CeciliaCunha](https://github.com/CeciliaCunha))
- **Julia Patricio** ([@juliapat18](https://github.com/juliapat18))
- **Marcella Anderle** ([@marcellaanderle](https://github.com/marcellaanderle))

---

## 📄 Licença

Este projeto é distribuído sob os termos da **MIT License**. Consulte o arquivo [LICENSE](LICENSE) para detalhes.
