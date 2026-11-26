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

### macOS (via Homebrew)

```bash
brew install cmake flex bison
# Exporte o Bison do Homebrew para usar uma versão moderna no lugar da legada da Apple
export PATH="$(brew --prefix bison)/bin:$PATH"
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

```bash
git config --global user.name "Seu Nome"
git config --global user.email "seu.email@exemplo.com"

git clone https://github.com/Interpretador-Musical/Grupo03-COMP01.git
cd Grupo03-COMP01
```

> **Dica para Windows + WSL:** se o repositório ficar em `/mnt/c/...`, prefira rodar operações Git via `git.exe` dentro do WSL (ou no PowerShell) para evitar conflitos de `.git/index.lock` com editores abertos no Windows.

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

# 1. Análise léxica (Flex)
./build/compilador --tokens exemplos/arpejo.mus

# 2. Análise semântica e tabela de símbolos
./build/compilador --simbolos exemplos/arpejo.mus

# 3. Representação Intermediária (Timeline em CSV) e comparação com/sem dobra (COMP-D)
./build/compilador -q --emit-timeline exemplos/arpejo.mus
./build/compilador -q --sem-otimizacao --emit-timeline exemplos/arpejo.mus

# 4. Saída-alvo musical offline em WAV PCM 16-bit (COMP-C) — sem precisar de placa de som
./build/compilador -o arpejo.wav exemplos/arpejo.mus
./build/compilador --saida arpejo.wav --volume 0.5 exemplos/arpejo.mus

# 5. Validação sem áudio e execução ao vivo
./build/compilador --no-audio exemplos/arpejo.mus
./build/compilador exemplos/arpejo.mus
```

### Executando a suíte de testes automatizados

```bash
# Todos os 35 testes do CTest (unitários, IR, CLI --emit-timeline, CLI -o/--saida, integração, concorrência e ocioso)
ctest --test-dir build --output-on-failure

# Diretamente pelo binário doctest (excluindo ou filtrando suítes demoradas)
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
