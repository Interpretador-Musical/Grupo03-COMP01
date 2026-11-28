---
title: Validação e Notas da Release M2
layout: default
parent: Documentação
nav_order: 18
---

# INT-08 — Validação e Lançamento da Release M2 (Beta 0.5)

**Versão:** `0.5.0` (`v0.5.0-m2` — Pre-release Beta 0.5)

## 1. Objetivo

A INT-08 (#48) estabelece um processo reproduzível para compilar, validar, empacotar e lançar a versão beta formal (**Beta 0.5 — `v0.5.0-m2`**) do Compilador Musical, consolidando a evolução das Sprints 3, 4 e 5 desde a [Release M1 (`v0.1.0-m1`)]({{ site.baseurl }}{% link release_m1.md %}).

## 2. Ambiente e requisitos

O projeto utiliza:

- C++17;
- CMake (3.16+);
- Flex e Bison;
- Git;
- Compilador C++ compatível (GCC ou Clang).

No Windows, o ambiente de desenvolvimento recomendado é o WSL com Ubuntu.

## 3. Validação e empacotamento automatizados

### 3.1. Script de validação (`validar_release.sh`)

A validação completa pode ser executada pela raiz do repositório:

```bash
bash scripts/validar_release.sh
```

O script interrompe a execução na primeira falha (`set -Eeuo pipefail`) e percorre seis etapas:

1. Verificação das ferramentas necessárias (`cmake`, `ctest`, `flex`, `bison`).
2. Configuração CMake em modo `Release` (`-DMUS_WERROR=ON -DMUS_BUILD_TESTS=ON`).
3. Compilação otimizada tratando avisos como erro.
4. Execução integral da suíte `CTest`.
5. Verificação do executável com `--help` e conferência de `--version` (`interpretador musical 0.5.0`).
6. Teste de portabilidade em diretório temporário limpo (`mktemp -d`), copiando apenas o binário, `samples/` e `exemplos/`, gerando WAV com síntese e sample (`"PERC_09"`) fora da árvore do repositório, confirmando que os samples foram lidos do diretório empacotado (e não de `MUS_SOURCE_DIR`) e validando a falha determinística para `MUS_SAMPLES_DIR` inválido.

### 3.2. Script de empacotamento (`empacotar_release.sh`)

Para gerar e homologar o pacote distribuível da Beta 0.5:

```bash
bash scripts/empacotar_release.sh
```

O script executa `validar_release.sh` (salvo quando `SKIP_VALIDATION=1`), monta `dist/compilador-v0.5.0-m2-<so>-<arquitetura>.tar.gz` contendo `compilador`, `samples/`, `exemplos/` e `README.md`, gera o arquivo `.sha256`, confere a soma de verificação, extrai o pacote em um diretório temporário isolado e renderiza composições `.mus` com samples e polifonia para `.wav`.

## 4. Validação no GitHub Actions

O workflow encontra-se em `.github/workflows/release-validation.yml`.

Ele executa `bash scripts/validar_release.sh` em matriz `ubuntu-latest` e `macos-latest`, em Pull Requests ou por acionamento manual (`workflow_dispatch`), com cancelamento automático de execuções obsoletas da mesma branch (`concurrency`). O workflow apenas valida o artefato e não publica releases automaticamente.

## 5. Evidências de validação

Configuração comum:

- `CMAKE_BUILD_TYPE=Release`
- `MUS_BUILD_TESTS=ON`
- `MUS_WERROR=ON`

### 5.1. Validação inicial no WSL (21/11/2026 — PR #146)

Ambiente: Ubuntu no WSL com repositório montado em `/mnt/c/` (penalidade de I/O entre sistemas de arquivos Windows/Linux), GCC 15.2.0, Flex 2.6.4 e Bison 3.8.2.

| Verificação | Resultado |
|---|---|
| Compilação Release | Aprovada |
| CTest — primeira execução | 22/22 aprovados (`340,32 s` no `/mnt/c/` do WSL) |
| CTest pelo script (`-j 1`) | 22/22 aprovados (`733,46 s` no `/mnt/c/` do WSL) |
| Revisão em macOS nativo (PR #146) | 22/22 aprovados (`~25 s` no script completo) |
| Verificação de `--help` | Aprovada |

> **Nota sobre o WSL:** os tempos de `340,32 s` e `733,46 s` decorrem da leitura repetida dos 85 arquivos WAV através da ponte `/mnt/c/` do WSL em cada teste de integração da CLI, e não representam o desempenho nativo do compilador.

### 5.2. Validação final em Linux nativo (27/11/2026 — fechamento da Beta 0.5)

Ambiente: Linux `x86_64` em sistema de arquivos nativo, GCC 15.2.0, Flex 2.6.4 e Bison 3.8.2.

| Verificação | Resultado |
|---|---|
| Compilação Release (`MUS_WERROR=ON`) | Aprovada (0 avisos) |
| Suíte `CTest` completa (`JOBS=4`) | **39/39 aprovados** em `16,29 s` |
| Análise estática (`cppcheck` em `src/`) | Aprovada (0 ocorrências) |
| Verificação de `--help` e `--version` | Aprovada (`interpretador musical 0.5.0`) |
| Execução portável em diretório limpo | Aprovada (`samples/` resolvido localmente fora do repositório) |
| Pacote `compilador-v0.5.0-m2-linux-x86_64.tar.gz` | Gerado (`240 MB`), checksum SHA-256 conferido e testado em `/tmp` |

### 5.3. Verificação de memória e concorrência (Sanitizers)

Além da validação Release, o workflow principal `.github/workflows/ci.yml` e os testes locais instrumentados cobrem:

- **AddressSanitizer (ASan) + LeakSanitizer (LSan):** verifica erros de acesso à memória, uso da arena da AST (`MUS_COM_ASAN=1`) e vazamentos na suíte `mus_tests` e na execução da CLI sobre `exemplos/*.mus`.
- **UndefinedBehaviorSanitizer (UBSan):** identifica estouros, conversões inválidas e comportamento indefinido nos caminhos exercitados.
- **ThreadSanitizer (TSan):** executa tanto a suíte instrumentada (`mus_tests`) quanto o próprio executável `compilador` tocando áudio real via backend `null` do `miniaudio` em thread dedicada (`SCH-08`, #46 / PR #144) nos cenários `exemplos/escala.mus`, `exemplos/arpejo.mus`, `exemplos/polirritmia.mus`, `--test-tone` e `--loop` encerrado por `SIGINT`, exigindo `"dispositivo de áudio pronto"`, ausência de alertas do TSan e código de saída `0` — ver [Concorrência e ThreadSanitizer]({{ site.baseurl }}{% link concorrencia.md %}).

Esses resultados constituem evidências sólidas dos critérios de qualidade da INT-08 (#48), sem substituir testes adicionais em cenários não cobertos pela suíte.

## 6. Portabilidade, consumo e limitações conhecidas

### 6.1. Carregamento de samples e custo de memória

Em toda execução que interpreta um arquivo `.mus`, o compilador executa `carregarSamples()` na inicialização para validar o diretório configurado e carregar os **85 samples WAV** do banco padrão (`INT-05`, #31). Essa política *fail-fast* garante diagnóstico imediato caso `MUS_SAMPLES_DIR` ou a pasta empacotada esteja ausente (comportamento fixado pelo teste `integracao_cli_recusa_samples_dir_invalido`), inclusive em scripts que não referenciam samples:

- **Tempo de inicialização:** cerca de `0,15 s` em sistemas de arquivos nativos (macOS/Linux Release), contra `13,98 s` quando o diretório está sobre `/mnt/c/` no WSL.
- **Consumo de memória (RSS):** conforme medido em [Testes de Estresse]({{ site.baseurl }}{% link estresse.md %}) (seção 6), o banco completo de 85 samples WAV decodificados estabelece uma base de aproximadamente **249 MB de RAM (RSS)**, acrescida de cerca de **100 bytes por evento** na Timeline.

### 6.2. Resolução portável do diretório `samples/`

Na etapa inicial da INT-08 (PR #146), identificou-se que `carregarSamples()` dependia exclusivamente da constante de compilação `MUS_SOURCE_DIR`, impedindo a distribuição independente do binário.

Para a Release M2, `src/cli/sample_paths.h` e `src/cli/sample_paths.cpp` implementam a busca em tempo de execução na seguinte ordem de precedência:

1. Variável de ambiente `MUS_SAMPLES_DIR` (quando definida e não vazia; se apontar para caminho inexistente, falha imediatamente sem fallback silencioso);
2. `<dir_do_executavel>/samples` (layout do pacote `.tar.gz` distribuído);
3. `<dir_do_executavel>/../samples` (layout de build CMake fora da árvore, como `build/compilador`);
4. `<MUS_SOURCE_DIR>/samples` (fallback apenas para builds locais de desenvolvimento).

Uma pasta `./samples` no diretório de trabalho atual (`cwd`) **não** entra na busca para evitar que arquivos do diretório do usuário sombrem os WAVs do compilador; quem quiser usar outro diretório de samples deve definir explicitamente `MUS_SAMPLES_DIR`.

> **Importante:** o executável continua precisando dos arquivos `.wav` de `samples/` distribuídos junto ao pacote (ou apontados por `MUS_SAMPLES_DIR`). Se copiado sozinho para outra máquina sem `samples/`, o programa encerra de forma limpa informando o caminho local esperado (`<dir_do_executavel>/samples`).

### 6.3. Integrações consolidadas

Os PRs que estavam abertos na preparação inicial deste documento (#141 limites de execução, #143 dobra de constantes `COMP-D` e #144 TSan na CLI com áudio), bem como `CMP-11` (#150), `COMP-C` (#151), `BUG-AUDIO` (#152), `SCH-12` (#153), `DOC-05` (#154), `MAT-10` (#156), `DSP-09` (#157), `INT-11` (#158) e `DSP-11` (#159), foram integrados à `main` antes do fechamento da `v0.5.0-m2`.

---

## 7. Notas da Release M2 (`v0.5.0-m2` — Beta 0.5)

### O que mudou desde a Release M1 (`v0.1.0-m1`)

1. **Linguagem, Semântica e Otimização (`CMP` / `MAT`):**
   - Variáveis tipadas, expressões aritméticas/lógicas e laços `repita` com escopo léxico local; açúcar sintático de figuras rítmicas e `vez` (`CMP-11`); infraestrutura em C++ de ambientes léxicos e closures (`MAT-06`, mantendo `se`/`senao` e `defina`/`retorna` reservados fora da gramática `parser.y`);
   - Trilhas paralelas e acordes polifônicos com `{ trilha1, trilha2, ... }` preservando escopo local de estado musical (`CMP-07`);
   - Tabela de símbolos completa inspecionável por `--simbolos` (`CMP-13`) e limites seguros de execução para valores não finitos e teto de 5 milhões de eventos (#124, #125);
   - Alocação dos nós da AST em arena contígua (`MAT-10`) e otimização por dobra de constantes ativa por padrão com flag `--sem-otimizacao` para comparação (`COMP-D`).

2. **Representação Intermediária e Geração de Código (`COMP-B` / `COMP-C`):**
   - Emissão determinística da Timeline (IR) em CSV via `--emit-timeline`;
   - Renderização offline para arquivo WAV PCM 16-bit mono a 44,1 kHz via `-o` / `--saida <arquivo.wav>`, compartilhando o mesmo pipeline de síntese e master bus do motor em tempo real.

3. **Motor de Áudio, Samples e Processamento Digital de Sinais (`INT-05` / `DSP` / `SCH`):**
   - Carregamento de 85 samples WAV (`toca "PERC_09" por 1`) com busca portável em tempo de execução (`sample_paths`);
   - Mixagem polifônica otimizada por tabela de onda senoidal (`wavetable`), envelopes adaptativos para notas curtas (#123) e limitador de pico `softClip` contra saturação (`BUG-AUDIO`, #109);
   - Controle de ganho por subgrupo e volume master suavizado via `--volume <0..1>` (`DSP-09`);
   - Filtro passa-baixa BiQuad RBJ por voz (`filtro <hz>` na linguagem) e no master bus (`--lpf <hz>` na CLI, `DSP-11`);
   - Redução de consumo ocioso de CPU (`SCH-12`) e verificação contínua sob ASan, UBSan e TSan com áudio ativo (`SCH-08`).

### Como executar o pacote distribuído (`Linux x86_64`)

O arquivo distribuível `.tar.gz` tem cerca de **240 MB** compactados (**~339 MB** após a extração), pois inclui o executável junto aos 85 arquivos `.wav` da pasta `samples/`.

```bash
# 1. Conferir integridade e extrair o pacote
sha256sum -c compilador-v0.5.0-m2-linux-x86_64.tar.gz.sha256
tar -xzf compilador-v0.5.0-m2-linux-x86_64.tar.gz
cd compilador-v0.5.0-m2-linux-x86_64

# 2. Verificar versão e opções disponíveis
./compilador --version
./compilador --help

# 3. Reproduzir um exemplo com áudio ou exportar para WAV
./compilador exemplos/polirritmia.mus
./compilador -o polirritmia.wav --volume 0.85 --lpf 4000 exemplos/polirritmia.mus
```

---

## 8. Procedimento de publicação

1. Atualizar a branch `main` e confirmar os checks do GitHub Actions (`CI` e `Release Validation`).
2. Gerar e validar o artefato com `bash scripts/empacotar_release.sh`.
3. Criar e publicar a Pre-release `v0.5.0-m2` no GitHub anexando o arquivo `.tar.gz` e seu `.sha256`.
4. Conferir os ativos publicados e encerrar a issue #48.

## 9. Critérios de conclusão

- [x] Compilação Release validada localmente (`MUS_WERROR=ON`).
- [x] Testes CTest aprovados localmente (39/39).
- [x] Script de validação e script de empacotamento implementados.
- [x] Workflow validado no GitHub Actions (`ubuntu-latest` e `macos-latest`).
- [x] Pacote de distribuição portável validado em diretório limpo (`.tar.gz` + `SHA-256`).
- [x] Notas finais da versão Beta 0.5 (`v0.5.0-m2`) consolidadas.
- [x] Pre-release `v0.5.0-m2` pronta para publicação no GitHub na Etapa 5.
