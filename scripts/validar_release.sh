#!/usr/bin/env bash
set -Eeuo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT/build-release}"
JOBS="${JOBS:-2}"
VERSAO_ESPERADA="$(awk '/^project\(InterpretadorMusical VERSION / { print $3; exit }' "$ROOT/CMakeLists.txt")"
if [[ -z "$VERSAO_ESPERADA" ]]; then
    echo "ERRO: nao foi possivel extrair a versao de CMakeLists.txt" >&2
    exit 1
fi

echo "========================================"
echo " INT-08 - Validacao da Release M2"
echo "========================================"

echo "[1/6] Verificando ferramentas"
for ferramenta in cmake ctest flex bison; do
    if ! command -v "$ferramenta" >/dev/null 2>&1; then
        echo "ERRO: ferramenta ausente: $ferramenta" >&2
        exit 1
    fi
done

echo "[2/6] Configurando CMake Release"
cmake -S "$ROOT" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DMUS_WERROR=ON \
    -DMUS_BUILD_TESTS=ON

echo "[3/6] Compilando"
cmake --build "$BUILD_DIR" --parallel "$JOBS"

echo "[4/6] Executando CTest"
ctest --test-dir "$BUILD_DIR" \
    --output-on-failure \
    -j 1

echo "[5/6] Validando executavel e versao"

COMPILADOR="$BUILD_DIR/compilador"

if [[ ! -x "$COMPILADOR" ]]; then
    echo "ERRO: executavel nao encontrado" >&2
    exit 1
fi

"$COMPILADOR" --help >/dev/null

SAIDA_VERSAO="$("$COMPILADOR" --version)"
if [[ "$SAIDA_VERSAO" != "interpretador musical $VERSAO_ESPERADA" ]]; then
    echo "ERRO: versao inesperada: '$SAIDA_VERSAO' (esperado: 'interpretador musical $VERSAO_ESPERADA')" >&2
    exit 1
fi

echo "[6/6] Validando execucao portavel em diretorio limpo"
TMP_VALIDACAO="$(mktemp -d)"
trap 'rm -rf "$TMP_VALIDACAO"' EXIT

PACOTE_DIR="$TMP_VALIDACAO/pacote"
mkdir -p "$PACOTE_DIR/samples" "$PACOTE_DIR/exemplos"
cp "$COMPILADOR" "$PACOTE_DIR/compilador"
cp "$ROOT"/samples/*.wav "$PACOTE_DIR/samples/"
cp "$ROOT"/exemplos/*.mus "$PACOTE_DIR/exemplos/"

cat > "$TMP_VALIDACAO/teste_sample.mus" <<'EOF'
andamento 120
toca "PERC_09" por 1
toca do4 por 1
EOF

(
    cd "$TMP_VALIDACAO"
    unset MUS_SAMPLES_DIR
    "$PACOTE_DIR/compilador" -v -o "$TMP_VALIDACAO/saida_sample.wav" "$TMP_VALIDACAO/teste_sample.mus" \
        2> "$TMP_VALIDACAO/log_sample.txt"
)

PACOTE_SAMPLES_REAL="$(cd "$PACOTE_DIR/samples" && pwd -P)"
if ! grep -Fq "diretorio de samples: $PACOTE_SAMPLES_REAL" "$TMP_VALIDACAO/log_sample.txt"; then
    echo "ERRO: o executavel empacotado nao usou o diretorio local de samples ($PACOTE_SAMPLES_REAL)" >&2
    cat "$TMP_VALIDACAO/log_sample.txt" >&2
    exit 1
fi

if [[ ! -s "$TMP_VALIDACAO/saida_sample.wav" ]]; then
    echo "ERRO: WAV de validacao portavel nao foi gerado" >&2
    exit 1
fi

if MUS_SAMPLES_DIR="$TMP_VALIDACAO/diretorio_inexistente" \
   "$PACOTE_DIR/compilador" --no-audio "$PACOTE_DIR/exemplos/escala.mus" >/dev/null 2>&1; then
    echo "ERRO: MUS_SAMPLES_DIR invalido deveria falhar" >&2
    exit 1
fi

echo
echo "========================================"
echo " RELEASE VALIDADA COM SUCESSO"
echo "========================================"
echo "Versao: $VERSAO_ESPERADA"
echo "Build: Release"
echo "Warnings como erro: ON"
echo "Testes: aprovados"
echo "Portabilidade de samples: aprovada"
echo "Executavel: $COMPILADOR"
