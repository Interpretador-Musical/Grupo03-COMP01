#!/usr/bin/env python3
"""SCH-11: verificação estática de operações proibidas no callback de áudio."""

import argparse
import re
import sys
from pathlib import Path


# Funções auditadas: arquivo -> assinaturas.
# Não incluir construtores nem funções de inicialização.
TARGETS = {
    "src/audio/audio_engine.cpp": [
        "AudioEngine::dataCallback",
        "AudioEngine::render",
        "AudioEngine::renderSine",
        "AudioEngine::renderRealTime",
        "AudioEngine::allocateVoice",
        "AudioEngine::isBetterToSteal",
    ],
    "src/core/timeline_player.cpp": [
        "TimelinePlayer::render",
    ],
    "src/audio/audio_buffer.cpp": [
        "AudioBuffer::sampleLinear",
    ],
    "src/audio/sample_bank.cpp": [
        "SampleBank::get",
    ],
    "src/core/ring_buffer.h": [
        "pop",
    ],
    "src/audio/limiter.h": [
        "softClip",
        "softClipBuffer",
    ],
    "src/audio/wavetable.h": [
        "sineTable",
        "lookupSine",
        "frequencyToPhaseIncrement",
    ],
}

# A verificação distingue chamadas de funções e palavras-chave C++.
FORBIDDEN_CALLS = re.compile(
    r"\b(?:malloc|calloc|realloc|free|"
    r"printf|fprintf|sprintf|snprintf|"
    r"puts|fputs|fwrite|fopen|fclose|"
    r"sleep|usleep|sleep_for|"
    r"system|popen)\s*\("
)

FORBIDDEN_CONSTRUCTS = re.compile(
    r"\b(?:new|delete|mutex|recursive_mutex|"
    r"timed_mutex|shared_mutex|"
    r"lock_guard|unique_lock|scoped_lock|"
    r"condition_variable|condition_variable_any|"
    r"future|shared_future|"
    r"cout|cerr|clog|"
    r"wcout|wcerr|wclog)\b"
)

FORBIDDEN_LOCK_CALLS = re.compile(
    r"\b(?:lock|try_lock|unlock|wait|wait_for|wait_until)\s*\("
)

# Detecta variaveis static locais inicializadas por lambda.
# Em C++17, essa inicializacao dinamica pode exigir sincronizacao.
FORBIDDEN_STATIC_INIT = re.compile(
    r"\bstatic\s+(?:const\s+)?"
    r"[\w:<>+,\s]+\s+\w+\s*=\s*\[\s*\]\s*\("
    r"[^)]*\)\s*\{",
    re.MULTILINE,
)

# Detecta inicializacao de variaveis static locais por chamada de funcao.
# Nao corresponde a constantes literais como static const int valor = 42.
FORBIDDEN_STATIC_CALL_INIT = re.compile(
    r"\bstatic\s+(?:const\s+)?"
    r"[\w:<>+,\s]+\s+\w+\s*=\s*"
    r"(?:[\w:]+\s*\([^;{}]*\))\s*;"
)

# Remove comentários e literais preservando posições e quebras de linha.
TOKEN = re.compile(
    r"//[^\n]*"
    r"|/\*[\s\S]*?\*/"
    r'|"(?:\\.|[^"\\])*"'
    r"|'(?:\\.|[^'\\])*'",
)


def mask_comments_and_literals(source):
    def mask(match):
        value = match.group(0)
        return "".join("\n" if c == "\n" else " " for c in value)

    return TOKEN.sub(mask, source)


def extract_function(source, signature):
    """Localiza uma definição e extrai seu corpo com chaves balanceadas."""
    pattern = re.compile(
        r"\b" + re.escape(signature)
        + r"\s*\([^;{}]*\)\s*(?:const\s*)?\{"
    )

    matches = list(pattern.finditer(source))

    if len(matches) != 1:
        raise ValueError(
            f"esperada exatamente uma definição de {signature}; "
            f"encontradas {len(matches)}"
        )

    match = matches[0]
    start = match.end()
    depth = 1

    for index in range(start, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return start, source[start:index]

    raise ValueError(f"corpo incompleto: {signature}")


def audit(root):
    violations = []
    checked = 0

    for relative_path, signatures in TARGETS.items():
        path = root / relative_path

        if not path.is_file():
            violations.append(f"{relative_path}: arquivo não encontrado")
            continue

        source = mask_comments_and_literals(
            path.read_text(encoding="utf-8")
        )

        for signature in signatures:
            try:
                offset, body = extract_function(source, signature)
            except ValueError as error:
                violations.append(f"{relative_path}: {error}")
                continue

            checked += 1

            for pattern in (
                FORBIDDEN_CALLS,
                FORBIDDEN_CONSTRUCTS,
                FORBIDDEN_LOCK_CALLS,
                FORBIDDEN_STATIC_INIT,
                FORBIDDEN_STATIC_CALL_INIT,
            ):
                for match in pattern.finditer(body):
                    position = offset + match.start()
                    line = source.count("\n", 0, position) + 1
                    violations.append(
                        f"{relative_path}:{line}: "
                        f"{signature}: operação proibida: "
                        f"{match.group(0)}"
                    )

    return checked, violations


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--root",
        type=Path,
        default=Path(__file__).resolve().parent.parent,
        help="raiz do repositório",
    )
    args = parser.parse_args()

    checked, violations = audit(args.root.resolve())

    if violations:
        print("SCH-11: FALHA na auditoria Real-Time")
        for violation in violations:
            print(f"  {violation}", file=sys.stderr)
        return 1

    print(
        f"SCH-11: OK — {checked} funções verificadas; "
        "nenhuma operação proibida identificada."
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
