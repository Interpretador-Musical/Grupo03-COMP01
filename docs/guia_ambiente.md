---
title: Guia de Ambiente e Execução
layout: default
parent: Documentação
nav_order: 5
---

# Guia de Configuração de Ambiente, Compilação e Execução

O Compilador Musical (Grupo 03) é escrito em **C++17** e utiliza **CMake**, **GNU Flex** e **GNU Bison**. Em **Windows**, o ambiente recomendado e homologado é o **Windows Subsystem for Linux (WSL)** com Ubuntu; o projeto também compila e passa na CI em **Linux** nativo e **macOS**.

---

## 1. Instalação e Ativação do WSL (Windows)

1. Abra o **PowerShell** como Administrador no Windows.
2. Execute o comando de instalação automática:
   ```powershell
   wsl --install
   ```
3. Reinicie o computador caso o instalador solicite.
4. Após reiniciar, abra o menu Iniciar e procure por **Ubuntu** (ou **WSL**) para abrir o terminal Linux.

---

## 2. Instalação das Dependências (Flex, Bison, C++ e CMake)

### Ubuntu / Debian e Windows (via WSL)

O build não depende diretamente das bibliotecas de áudio do sistema, mas a `miniaudio` carrega ALSA e PulseAudio ao tocar som ao vivo; por isso instale `libasound2-dev` e `libpulse-dev`:

```bash
sudo apt update
sudo apt install flex bison build-essential cmake git libasound2-dev libpulse-dev -y
```

### macOS (Xcode Command Line Tools + Homebrew)

O compilador C++ (`clang`), o **Flex** e o **Bison 2.3** já acompanham o **Xcode Command Line Tools** (a gramática em `parser/parser.y` é compatível com o Bison 2.3 do sistema, mesmo ambiente utilizado pelo CI de macOS), bastando instalar o **CMake** via Homebrew:

```bash
xcode-select --install
brew install cmake
```

### Verificando as ferramentas

```bash
flex --version
bison --version
cmake --version
git --version
```

---

## 3. Clonagem e Configuração do Git

Em **Windows + WSL**, clone o repositório diretamente no sistema de arquivos Linux do WSL (por exemplo, em `~/Grupo03-COMP01`, dentro de `~`), e **não** sob `/mnt/c/`: misturar Git do Windows e do Linux no mesmo clone gera arquivos com `CRLF`, torna o I/O lento e faz scripts Bash como `bash scripts/validar_release.sh` falharem.

```bash
git config --global user.name "Seu Nome"
git config --global user.email "seu.email@exemplo.com"

cd ~
git clone https://github.com/Interpretador-Musical/Grupo03-COMP01.git
cd Grupo03-COMP01
```

---

## 4. Compilando, Executando e Testando

### Build padrão (Debug) e Build Release

```bash
# Build padrão
cmake -S . -B build
cmake --build build --parallel

# Build otimizado (Release) ou validação completa da Release M2 (INT-08)
cmake -S . -B build-rel -DCMAKE_BUILD_TYPE=Release
cmake --build build-rel --parallel
bash scripts/validar_release.sh
```

### Exercitando todas as fases do compilador

Com o executável em `./build/compilador`:

```bash
# Ajuda e depuração de dispositivo de áudio
./build/compilador --help
./build/compilador --test-tone              # senoide de 440 Hz por 3 s

# [Fase 1] Análise léxica (Flex)
./build/compilador --tokens exemplos/arpejo.mus

# [Fase 2] Análise sintática e AST em Arena (executada automaticamente antes da Fase 3;
#           erros léxicos ou sintáticos interrompem a compilação com linha e coluna)

# [Fase 3] Análise semântica e tabela de símbolos
./build/compilador --simbolos exemplos/arpejo.mus

# [Fase 4] Otimização da AST — dobra de constantes (COMP-D, ativa por padrão; compare com --sem-otimizacao)
./build/compilador -q --sem-otimizacao --emit-timeline exemplos/arpejo.mus

# [Fase 5] Representação Intermediária (Timeline em CSV determinístico, COMP-B)
./build/compilador -q --emit-timeline exemplos/arpejo.mus

# [Fase 6] Saída-alvo musical offline em WAV PCM 16-bit (COMP-C) — sem precisar de placa de som
./build/compilador -o arpejo.wav exemplos/arpejo.mus
./build/compilador --saida arpejo.wav --volume 0.5 exemplos/arpejo.mus

# Pipeline completo (Fases 1–5 + execução ao vivo na Fase 6, ou validação sem áudio)
./build/compilador --no-audio exemplos/arpejo.mus
./build/compilador exemplos/arpejo.mus
```

### Executando a suíte de testes automatizados

```bash
# Todos os 35 testes do CTest (unitários, IR, CLI --emit-timeline, CLI -o/--saida, integração, concorrência e ocioso)
ctest --test-dir build --output-on-failure

# Diretamente pelo binário doctest (506 casos na suíte padrão; 519 listados em mus_tests --count)
./build/tests/mus_tests
./build/tests/mus_tests --reporters=console --no-intro --test-suite-exclude=estresse,concorrencia,ocioso
./build/tests/mus_tests --test-suite=concorrencia
./build/tests/mus_tests --test-suite=ocioso
./build/tests/mus_tests --test-suite=estresse
```

---

## 5. Avisos, Análise Estática e Sanitizers

O CI reprova qualquer aviso do compilador, achados do Cppcheck (`CMP-12`, #64) e erros detectados por AddressSanitizer/UBSan ou ThreadSanitizer.

### Avisos estritos (`MUS_WERROR=ON`) e Cppcheck

```bash
cmake -S . -B build-werror -DCMAKE_BUILD_TYPE=Debug -DMUS_WERROR=ON
cmake --build build-werror --parallel

sudo apt install cppcheck -y
cppcheck --enable=warning,style,performance,portability \
  --std=c++17 --language=c++ --inline-suppr --error-exitcode=1 \
  --suppress=missingIncludeSystem \
  --suppress=useStlAlgorithm --suppress=shadowFunction \
  -q -I src src
```

O motivo de cada flag e supressão está documentado em [Análise estática]({{ site.baseurl }}{% link analise_estatica.md %}).

### AddressSanitizer + UBSan e ThreadSanitizer

```bash
# AddressSanitizer + LeakSanitizer + UndefinedBehaviorSanitizer
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug -DENABLE_ASAN=ON
cmake --build build-asan --parallel
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 \
  ./build-asan/tests/mus_tests

# ThreadSanitizer (suíte + executável tocando áudio)
cmake -S . -B build-tsan -DCMAKE_BUILD_TYPE=Debug -DENABLE_TSAN=ON
cmake --build build-tsan --target mus_tests compilador --parallel
TSAN_OPTIONS=halt_on_error=1 ./build-tsan/tests/mus_tests
```
