#!/usr/bin/env python3
"""SCH-11: verificação estática de operações proibidas no callback de áudio."""

import argparse
import re
import sys
from pathlib import Path


# Funções auditadas: arquivo -> assinaturas no caminho real de `dataCallback`.
# Não incluir construtores nem funções exclusivas de inicialização fora do áudio.
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
    "src/core/frames.h": [
        "segundosParaFrame",
    ],
    "src/core/sound_event.h": [
        "SoundEvent::endTime",
    ],
    "src/core/ring_buffer.h": [
        "SpscRingBuffer::pop",
        "SpscRingBuffer::advance",
    ],
    "src/audio/limiter.h": [
        "softClip",
        "softClipBuffer",
        "LimitadorDePico::processar",
        "LimitadorDePico::processarBuffer",
    ],
    "src/audio/gain_smoother.h": [
        "GainSmoother::snapTo",
        "GainSmoother::setTarget",
        "GainSmoother::next",
        "GainSmoother::advance",
        "GainSmoother::processBuffer",
    ],
    "src/audio/biquad_filter.h": [
        "sanitizeFilterCutoff",
        "sanitizeFilterQ",
        "BiquadFilter::resetState",
        "BiquadFilter::disable",
        "BiquadFilter::configureLowPass",
        "BiquadFilter::processSample",
        "BiquadFilter::flushDenormals",
        "BiquadFilter::processBuffer",
    ],
    "src/audio/wavetable.h": [
        "sineTable",
        "lookupSine",
        "frequencyToPhaseIncrement",
    ],
}

# Chamadas C/POSIX/STL de alocação, I/O ou suspensão da thread.
FORBIDDEN_CALLS = re.compile(
    r"\b(?:malloc|calloc|realloc|free|aligned_alloc|posix_memalign|"
    r"printf|fprintf|sprintf|snprintf|vprintf|vfprintf|"
    r"puts|fputs|fwrite|fread|fopen|fclose|fflush|"
    r"sleep|usleep|nanosleep|sleep_for|sleep_until|"
    r"system|popen)\s*\("
)

# Palavras-chave, smart pointers com alocação, exceções, primitivas bloqueantes e logs.
FORBIDDEN_CONSTRUCTS = re.compile(
    r"\b(?:new|delete|throw|"
    r"make_unique|make_shared|allocate_shared|"
    r"mutex|recursive_mutex|timed_mutex|shared_mutex|"
    r"lock_guard|unique_lock|scoped_lock|shared_lock|"
    r"condition_variable|condition_variable_any|"
    r"future|shared_future|promise|async|jthread|"
    r"cout|cerr|clog|cin|wcout|wcerr|wclog|wcin|"
    r"LOG_DEBUG|LOG_INFO|LOG_WARN|LOG_ERROR)\b"
)

# Tipos da biblioteca padrão que alocam no heap ou apagam tipo com heap potencial.
FORBIDDEN_STD_TYPES = re.compile(
    r"\bstd::(?:string|wstring|u16string|u32string|"
    r"function|vector|deque|list|forward_list|"
    r"map|multimap|unordered_map|unordered_multimap|"
    r"set|multiset|unordered_set|unordered_multiset|"
    r"stringstream|ostringstream|istringstream|"
    r"fstream|ifstream|ofstream|thread)\b"
)

# Operações de crescimento/realocação em containers dinâmicos.
FORBIDDEN_CONTAINER_OPS = re.compile(
    r"(?:->|\.)\s*(?:push_back|emplace_back|push_front|emplace_front|"
    r"emplace|insert|resize|reserve|shrink_to_fit|assign)\s*\("
)

FORBIDDEN_LOCK_CALLS = re.compile(
    r"\b(?:lock|try_lock|unlock|wait|wait_for|wait_until)\s*\("
)

# Declarações `static` locais (com avaliação filtrada para permitir `constexpr`).
LOCAL_STATIC_DECL = re.compile(r"\bstatic\b[^;{]*[;{]")

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


def _extract_balanced_block(source, start):
    """Extrai o trecho entre `{` (já consumido em `start - 1`) e o `}` correspondente."""
    depth = 1
    for index in range(start, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return start, source[start:index]
    return None


def _extract_class_body(source, class_name):
    """Localiza o corpo de `class` ou `struct` em cabeçalhos, se definido neste arquivo."""
    pattern = re.compile(
        r"\b(?:class|struct)\s+" + re.escape(class_name) + r"\b[^{;]*\{"
    )
    matches = list(pattern.finditer(source))
    if len(matches) != 1:
        return None
    block = _extract_balanced_block(source, matches[0].end())
    if block is None:
        raise ValueError(f"corpo de classe incompleto: {class_name}")
    return block


def extract_function(source, signature):
    """Localiza uma definição de função/método e extrai seu corpo com chaves balanceadas."""
    def build_pattern(name):
        return re.compile(
            r"\b" + re.escape(name)
            + r"\s*\([^;{}()]*\)\s*(?:const\s*)?(?:noexcept(?:\s*\([^)]*\))?\s*)?\{"
        )

    matches = list(build_pattern(signature).finditer(source))
    base_offset = 0
    search_space = source

    # Se for `Classe::metodo` inline dentro da definição da classe num `.h`.
    if len(matches) == 0 and "::" in signature:
        class_name, method_name = signature.rsplit("::", 1)
        class_block = _extract_class_body(source, class_name)
        if class_block is not None:
            base_offset, search_space = class_block
            matches = list(build_pattern(method_name).finditer(search_space))

    if len(matches) != 1:
        raise ValueError(
            f"esperada exatamente uma definição de {signature}; "
            f"encontradas {len(matches)}"
        )

    block = _extract_balanced_block(search_space, matches[0].end())
    if block is None:
        raise ValueError(f"corpo incompleto: {signature}")

    rel_offset, body = block
    return base_offset + rel_offset, body


def find_non_constexpr_local_statics(body):
    """Detecta variáveis `static` locais sem `constexpr` (sujeitas a `__cxa_guard_acquire`)."""
    for match in LOCAL_STATIC_DECL.finditer(body):
        snippet = match.group(0)
        decl_head = re.split(r"[=({]", snippet, maxsplit=1)[0]
        prefix = body[max(0, match.start() - 32):match.start()]
        if re.search(r"\bconstexpr\b", decl_head) or re.search(r"\bconstexpr\s+$", prefix):
            continue
        yield match


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
                FORBIDDEN_STD_TYPES,
                FORBIDDEN_CONTAINER_OPS,
                FORBIDDEN_LOCK_CALLS,
            ):
                for match in pattern.finditer(body):
                    position = offset + match.start()
                    line = source.count("\n", 0, position) + 1
                    violations.append(
                        f"{relative_path}:{line}: "
                        f"{signature}: operação proibida: "
                        f"{match.group(0).strip()}"
                    )

            for match in find_non_constexpr_local_statics(body):
                position = offset + match.start()
                line = source.count("\n", 0, position) + 1
                violations.append(
                    f"{relative_path}:{line}: "
                    f"{signature}: operação proibida: "
                    f"{match.group(0).strip()}"
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
        print("SCH-11: FALHA na auditoria Real-Time", file=sys.stderr)
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
