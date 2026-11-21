#!/usr/bin/env bash
set -Eeuo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT/build-release}"
JOBS="${JOBS:-2}"

echo "========================================"
echo " INT-08 - Validacao da Release M2"
echo "========================================"

echo "[1/5] Verificando ferramentas"
for ferramenta in cmake ctest flex bison; do
    if ! command -v "$ferramenta" >/dev/null 2>&1; then
        echo "ERRO: ferramenta ausente: $ferramenta" >&2
        exit 1
    fi
done

echo "[2/5] Configurando CMake Release"
cmake -S "$ROOT" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DMUS_WERROR=ON \
    -DMUS_BUILD_TESTS=ON

echo "[3/5] Compilando"
cmake --build "$BUILD_DIR" --parallel "$JOBS"

echo "[4/5] Executando CTest"
ctest --test-dir "$BUILD_DIR" \
    --output-on-failure \
    -j 1

echo "[5/5] Validando executavel"

COMPILADOR="$BUILD_DIR/compilador"

if [[ ! -x "$COMPILADOR" ]]; then
    echo "ERRO: executavel nao encontrado" >&2
    exit 1
fi

"$COMPILADOR" --help >/dev/null

echo
echo "========================================"
echo " RELEASE VALIDADA COM SUCESSO"
echo "========================================"
echo "Build: Release"
echo "Warnings como erro: ON"
echo "Testes: aprovados"
echo "Executavel: $COMPILADOR"
