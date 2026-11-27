#!/usr/bin/env python3
"""Testes de regressão para o verificador Real-Time da SCH-11 (#61)."""

import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from check_realtime import TARGETS, audit, mask_comments_and_literals


SOURCE_ROOT = Path(__file__).resolve().parent.parent
SCRIPT_PATH = Path(__file__).resolve().parent / "check_realtime.py"


class RealtimeAuditTests(unittest.TestCase):

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

        for relative in TARGETS:
            source = SOURCE_ROOT / relative
            destination = self.root / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, destination)

    def inject(
        self,
        code,
        relative_path="src/audio/audio_engine.cpp",
        marker="AudioEngine::renderSine(",
    ):
        path = self.root / relative_path
        source = path.read_text(encoding="utf-8")

        start = source.index(marker)
        opening = source.index("{", start)

        source = (
            source[:opening + 1]
            + "\n    " + code + "\n"
            + source[opening + 1:]
        )

        path.write_text(source, encoding="utf-8")

    def test_codigo_original_aprovado(self):
        checked, violations = audit(self.root)

        self.assertEqual(checked, sum(map(len, TARGETS.values())))
        self.assertEqual(violations, [])

    def test_malloc_reprovado(self):
        self.inject("malloc(128);")
        _, violations = audit(self.root)
        self.assertTrue(any("malloc" in v for v in violations))

    def test_new_reprovado(self):
        self.inject("auto* value = new int(42);")
        _, violations = audit(self.root)
        self.assertTrue(any("new" in v for v in violations))

    def test_delete_reprovado(self):
        self.inject("delete value;")
        _, violations = audit(self.root)
        self.assertTrue(any("delete" in v for v in violations))

    def test_make_unique_reprovado(self):
        self.inject("auto p = std::make_unique<int>(1);")
        _, violations = audit(self.root)
        self.assertTrue(any("make_unique" in v for v in violations))

    def test_make_shared_reprovado(self):
        self.inject("auto p = std::make_shared<int>(1);")
        _, violations = audit(self.root)
        self.assertTrue(any("make_shared" in v for v in violations))

    def test_push_back_reprovado(self):
        self.inject("buf.push_back(1.0f);")
        _, violations = audit(self.root)
        self.assertTrue(any("push_back" in v for v in violations))

    def test_emplace_back_reprovado(self):
        self.inject("buf.emplace_back(1.0f);")
        _, violations = audit(self.root)
        self.assertTrue(any("emplace_back" in v for v in violations))

    def test_resize_reprovado(self):
        self.inject("buf.resize(4096);")
        _, violations = audit(self.root)
        self.assertTrue(any("resize" in v for v in violations))

    def test_reserve_reprovado(self):
        self.inject("buf.reserve(4096);")
        _, violations = audit(self.root)
        self.assertTrue(any("reserve" in v for v in violations))

    def test_insert_reprovado(self):
        self.inject("buf.insert(buf.begin(), 1.0f);")
        _, violations = audit(self.root)
        self.assertTrue(any("insert" in v for v in violations))

    def test_std_string_reprovada(self):
        self.inject('std::string s = "x";')
        _, violations = audit(self.root)
        self.assertTrue(any("std::string" in v for v in violations))

    def test_std_function_reprovada(self):
        self.inject("std::function<void()> fn = [this]() {};")
        _, violations = audit(self.root)
        self.assertTrue(any("std::function" in v for v in violations))

    def test_throw_reprovado(self):
        self.inject('throw std::runtime_error("x");')
        _, violations = audit(self.root)
        self.assertTrue(any("throw" in v for v in violations))

    def test_mutex_reprovado(self):
        self.inject("std::mutex mutex;")
        _, violations = audit(self.root)
        self.assertTrue(any("mutex" in v for v in violations))

    def test_lock_reprovado(self):
        self.inject("mutex.lock();")
        _, violations = audit(self.root)
        self.assertTrue(any("lock" in v for v in violations))

    def test_printf_reprovado(self):
        self.inject('printf("audio");')
        _, violations = audit(self.root)
        self.assertTrue(any("printf" in v for v in violations))

    def test_cout_reprovado(self):
        self.inject('std::cout << "audio";')
        _, violations = audit(self.root)
        self.assertTrue(any("cout" in v for v in violations))

    def test_macro_logger_reprovada(self):
        self.inject('LOG_WARN << "callback";')
        _, violations = audit(self.root)
        self.assertTrue(any("LOG_WARN" in v for v in violations))

    def test_condition_variable_reprovada(self):
        self.inject("std::condition_variable cv;")
        _, violations = audit(self.root)
        self.assertTrue(any("condition_variable" in v for v in violations))

    def test_sleep_reprovado(self):
        self.inject("std::this_thread::sleep_for(std::chrono::milliseconds(1));")
        _, violations = audit(self.root)
        self.assertTrue(any("sleep_for" in v for v in violations))

    def test_comentarios_e_strings_ignorados(self):
        source = '''
            // malloc(10);
            /* std::mutex mutex; */
            const char* texto = "printf(123)";
        '''
        masked = mask_comments_and_literals(source)

        self.assertNotIn("malloc", masked)
        self.assertNotIn("mutex", masked)
        self.assertNotIn("printf", masked)

    def test_comentario_na_funcao_ignorado(self):
        self.inject("// malloc(128);")
        _, violations = audit(self.root)
        self.assertEqual(violations, [])

    def test_alocacao_fora_do_callback_permitida(self):
        path = self.root / "src/audio/audio_engine.cpp"
        source = path.read_text(encoding="utf-8")
        self.assertIn("new ma_device()", source)

        checked, violations = audit(self.root)
        self.assertEqual(checked, sum(map(len, TARGETS.values())))
        self.assertEqual(violations, [])

    def test_funcao_ausente_reprovada(self):
        path = self.root / "src/audio/audio_engine.cpp"
        source = path.read_text(encoding="utf-8")
        source = source.replace(
            "AudioEngine::renderSine(",
            "AudioEngine::renderSineRenamed(",
            1,
        )
        path.write_text(source, encoding="utf-8")

        _, violations = audit(self.root)
        self.assertTrue(any("renderSine" in v for v in violations))

    def test_arquivo_ausente_reprovado(self):
        path = self.root / "src/audio/limiter.h"
        path.unlink()

        _, violations = audit(self.root)
        self.assertTrue(any("arquivo não encontrado" in v for v in violations))

    def test_inicializacao_estatica_na_wavetable_reprovada(self):
        self.inject(
            "static const int valor = []() { return 42; }();",
            relative_path="src/audio/wavetable.h",
            marker="inline float lookupSine(std::uint32_t phase)",
        )
        _, violations = audit(self.root)
        self.assertTrue(
            any("lookupSine" in v and "static" in v for v in violations)
        )

    def test_inicializacao_estatica_por_funcao_reprovada(self):
        self.inject(
            "static int valor = calcularValor();",
            relative_path="src/audio/wavetable.h",
            marker="inline float lookupSine(std::uint32_t phase)",
        )
        _, violations = audit(self.root)
        self.assertTrue(
            any("lookupSine" in v and "static" in v for v in violations)
        )

    def test_static_local_construtor_parenteses_reprovado(self):
        self.inject("static BufferLocal v(1024);")
        _, violations = audit(self.root)
        self.assertTrue(any("static" in v for v in violations))

    def test_static_local_chaves_reprovado(self):
        self.inject("static float tabela[]{1.0f, 2.0f};")
        _, violations = audit(self.root)
        self.assertTrue(any("static" in v for v in violations))

    def test_static_local_padrao_reprovado(self):
        self.inject("static Foo foo;")
        _, violations = audit(self.root)
        self.assertTrue(any("static Foo foo;" in v for v in violations))

    def test_static_constexpr_local_permitido(self):
        self.inject(
            "static constexpr int kConst = 42;\n"
            "    constexpr static float kEscala = 0.5f;"
        )
        _, violations = audit(self.root)
        self.assertEqual(violations, [])

    def test_modulos_inline_do_master_bus_auditados(self):
        self.inject(
            "malloc(64);",
            relative_path="src/audio/limiter.h",
            marker="float processar(float amostra)",
        )
        self.inject(
            "malloc(64);",
            relative_path="src/audio/gain_smoother.h",
            marker="void processBuffer(float* samples, std::uint32_t frameCount)",
        )
        self.inject(
            "malloc(64);",
            relative_path="src/audio/biquad_filter.h",
            marker="float processSample(float x)",
        )
        _, violations = audit(self.root)
        self.assertTrue(any("LimitadorDePico::processar" in v for v in violations))
        self.assertTrue(any("GainSmoother::processBuffer" in v for v in violations))
        self.assertTrue(any("BiquadFilter::processSample" in v for v in violations))

    def test_cli_envia_cabecalho_e_violacoes_para_stderr(self):
        self.inject("malloc(32);")
        proc = subprocess.run(
            [sys.executable, "-B", str(SCRIPT_PATH), "--root", str(self.root)],
            capture_output=True,
            text=True,
            check=False,
        )
        self.assertEqual(proc.returncode, 1)
        self.assertEqual(proc.stdout, "")
        self.assertIn("SCH-11: FALHA na auditoria Real-Time", proc.stderr)
        self.assertIn("malloc", proc.stderr)


if __name__ == "__main__":
    unittest.main()
