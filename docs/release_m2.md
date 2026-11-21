---
title: Validação da Release M2
layout: default
parent: Documentação
---

# INT-08 — Validação e preparação da Release M2

## 1. Objetivo

A INT-08 (#48) estabelece um processo reproduzível para compilar, testar e preparar uma versão beta do Compilador Musical.

A publicação da release depende da aprovação do grupo e da conclusão das integrações consideradas necessárias.

## 2. Ambiente e requisitos

O projeto utiliza:

- C++17;
- CMake;
- Flex e Bison;
- Git;
- Compilador C++ compatível.

No Windows, o ambiente de desenvolvimento recomendado é o WSL com Ubuntu.

## 3. Validação automatizada

A validação pode ser executada pela raiz do repositório:

```bash
bash scripts/validar_release.sh
```

O script executa:

1. Verificação das ferramentas necessárias.
2. Configuração CMake em modo Release.
3. Compilação com `MUS_WERROR=ON`.
4. Execução da suíte CTest.
5. Verificação do executável com `--help`.

A validação é interrompida quando alguma etapa falha.

## 4. Validação no GitHub Actions

O workflow encontra-se em `.github/workflows/release-validation.yml`.

Ele executa o processo de validação em Ubuntu e macOS, em Pull Requests ou por acionamento manual.

O workflow não publica releases automaticamente.

## 5. Evidências locais

Data da validação: 21/11/2026.

Ambiente: Ubuntu no WSL, GCC 15.2.0, Flex 2.6.4 e Bison 3.8.2.

Configuração:

- `CMAKE_BUILD_TYPE=Release`
- `MUS_BUILD_TESTS=ON`
- `MUS_WERROR=ON`

| Verificação | Resultado |
|---|---|
| Compilação Release | Aprovada |
| CTest — primeira execução | 22/22 aprovados |
| Tempo da primeira execução | 340,32 s |
| CTest pelo script | 22/22 aprovados |
| Tempo da segunda execução | 733,46 s |
| Verificação de `--help` | Aprovada |

Esses resultados comprovam a validação local no ambiente descrito. Não substituem os resultados da CI ou os testes de distribuição do pacote.

## 6. Limitações conhecidas

### 6.1. Carregamento de samples

O compilador carrega os samples WAV mesmo ao executar programas que utilizam somente notas sintetizadas.

Foram observados 85 samples carregados e um tempo de 13,98 segundos para executar:

```bash
./build-release/compilador --no-audio exemplos/escala.mus
```

O tempo pode variar conforme o hardware e o sistema de arquivos.

### 6.2. Portabilidade do executável

A configuração atual utiliza `MUS_SOURCE_DIR` para localizar o diretório de samples.

Esse valor é definido durante a compilação. Portanto, copiar somente o executável para outra máquina não garante a localização correta dos arquivos WAV.

A distribuição independente deve ser validada antes da publicação.

### 6.3. Integrações pendentes

Na preparação inicial desta documentação, os PRs #141, #143 e #144 permaneciam abertos.

O estado dessas integrações deve ser conferido novamente antes da publicação da release.

## 7. Procedimento de publicação

Após a integração e aprovação do grupo:

1. Atualizar a branch `main`.
2. Executar a validação Release no commit definitivo.
3. Confirmar os resultados do GitHub Actions.
4. Definir a tag e a versão da M2.
5. Preparar o pacote para a plataforma escolhida.
6. Testar o pacote em ambiente limpo.
7. Preparar as notas de versão.
8. Publicar a GitHub Release.
9. Conferir os arquivos publicados.
10. Atualizar a issue #48 e o GitHub Projects.

Não publicar o executável isoladamente como pacote portável sem testar a localização dos samples.

## 8. Critérios de conclusão

- [x] Compilação Release validada localmente.
- [x] Testes CTest aprovados localmente.
- [x] Script de validação implementado.
- [ ] Workflow validado no GitHub Actions.
- [ ] Pacote de distribuição validado.
- [ ] Notas finais da versão aprovadas.
- [ ] Release publicada no GitHub.
