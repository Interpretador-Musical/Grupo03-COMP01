#!/usr/bin/env bash
set -Eeuo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT/build-release}"
DIST_DIR="${DIST_DIR:-$ROOT/dist}"
JOBS="${JOBS:-2}"
RELEASE_TAG="${RELEASE_TAG:-v0.5.0-m2}"

calcular_sha256() {
    local arquivo="$1"
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum "$arquivo"
    elif command -v shasum >/dev/null 2>&1; then
        shasum -a 256 "$arquivo"
    else
        echo "ERRO: nem sha256sum nem shasum disponiveis" >&2
        exit 1
    fi
}

verificar_sha256() {
    local arquivo_hash="$1"
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum -c "$arquivo_hash"
    else
        shasum -a 256 -c "$arquivo_hash"
    fi
}

if [[ "${SKIP_VALIDATION:-0}" != "1" ]]; then
    BUILD_DIR="$BUILD_DIR" JOBS="$JOBS" bash "$ROOT/scripts/validar_release.sh"
fi

COMPILADOR="$BUILD_DIR/compilador"
if [[ ! -x "$COMPILADOR" ]]; then
    echo "ERRO: executavel Release nao encontrado em $COMPILADOR" >&2
    exit 1
fi

SO_BRUTO="$(uname -s)"
case "$SO_BRUTO" in
    Linux)  SO_TAG="linux" ;;
    Darwin) SO_TAG="macos" ;;
    *)      SO_TAG="$(echo "$SO_BRUTO" | tr '[:upper:]' '[:lower:]')" ;;
esac

ARQ_TAG="$(uname -m)"
NOME_PACOTE="compilador-${RELEASE_TAG}-${SO_TAG}-${ARQ_TAG}"
STAGE_DIR="$DIST_DIR/$NOME_PACOTE"
ARQUIVO_TAR="$DIST_DIR/${NOME_PACOTE}.tar.gz"
ARQUIVO_SHA="$DIST_DIR/${NOME_PACOTE}.tar.gz.sha256"

echo
echo "========================================"
echo " Empacotando $NOME_PACOTE"
echo "========================================"

rm -rf "$STAGE_DIR" "$ARQUIVO_TAR" "$ARQUIVO_SHA"
mkdir -p "$STAGE_DIR/samples" "$STAGE_DIR/exemplos"

cp "$COMPILADOR" "$STAGE_DIR/compilador"
chmod 755 "$STAGE_DIR/compilador"
cp "$ROOT"/samples/*.wav "$STAGE_DIR/samples/"
cp "$ROOT"/exemplos/*.mus "$STAGE_DIR/exemplos/"
cp "$ROOT/README.md" "$STAGE_DIR/README.md"

(
    cd "$DIST_DIR"
    tar -czf "${NOME_PACOTE}.tar.gz" "$NOME_PACOTE"
    calcular_sha256 "${NOME_PACOTE}.tar.gz" > "${NOME_PACOTE}.tar.gz.sha256"
)

rm -rf "$STAGE_DIR"

echo "Verificando integridade e extracao em ambiente limpo..."
TMP_TESTE="$(mktemp -d)"
trap 'rm -rf "$TMP_TESTE"' EXIT

(
    cd "$DIST_DIR"
    verificar_sha256 "${NOME_PACOTE}.tar.gz.sha256"
)

tar -xzf "$ARQUIVO_TAR" -C "$TMP_TESTE"

cat > "$TMP_TESTE/composicao_beta.mus" <<'EOF'
andamento 110
filtro 1800
toca "PERC_09" por 0.5
toca "BASS_1" por 0.5
{ toca do4 por 1, toca mi4 por 1, toca sol4 por 1 }
EOF

(
    cd "$TMP_TESTE"
    unset MUS_SAMPLES_DIR
    "./$NOME_PACOTE/compilador" --version
    "./$NOME_PACOTE/compilador" -v -o "$TMP_TESTE/composicao_beta.wav" "$TMP_TESTE/composicao_beta.mus" \
        2> "$TMP_TESTE/log_pacote.txt"
    "./$NOME_PACOTE/compilador" -q -o "$TMP_TESTE/polirritmia.wav" "./$NOME_PACOTE/exemplos/polirritmia.mus"
)

SAMPLES_EXTRAIDO="$(cd "$TMP_TESTE/$NOME_PACOTE/samples" && pwd -P)"
if ! grep -Fq "diretorio de samples: $SAMPLES_EXTRAIDO" "$TMP_TESTE/log_pacote.txt"; then
    echo "ERRO: pacote extraido nao carregou samples do proprio diretorio ($SAMPLES_EXTRAIDO)" >&2
    cat "$TMP_TESTE/log_pacote.txt" >&2
    exit 1
fi

for wav_gerado in "$TMP_TESTE/composicao_beta.wav" "$TMP_TESTE/polirritmia.wav"; do
    if [[ ! -s "$wav_gerado" ]]; then
        echo "ERRO: arquivo WAV esperado nao foi gerado: $wav_gerado" >&2
        exit 1
    fi
done

echo
echo "========================================"
echo " PACOTE PRONTO E VALIDADO"
echo "========================================"
echo "Arquivo:  $ARQUIVO_TAR"
echo "Checksum: $(cat "$ARQUIVO_SHA")"
