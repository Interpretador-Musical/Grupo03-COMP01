
#!/usr/bin/env python3
"""Testes de regressao para o verificador SCH-11."""

import shutil
import tempfile
import unittest
from pathlib import Path

from check_realtime import TARGETS, audit, mask_comments_and_literals


SOURCE_ROOT = Path(__file__).resolve().parent.parent


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

    def inject(self, code, function="AudioEngine::renderSine"):
        path = self.root / "src/audio/audio_engine.cpp"
        source = path.read_text(encoding="utf-8")

        marker = function + "("
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

        self.assertTrue(
            any("malloc" in violation for violation in violations)
        )

    def test_new_reprovado(self):
        self.inject("auto* value = new int(42);")
        _, violations = audit(self.root)

        self.assertTrue(
            any("new" in violation for violation in violations)
        )

    def test_delete_reprovado(self):
        self.inject("delete value;")
        _, violations = audit(self.root)

        self.assertTrue(
            any("delete" in violation for violation in violations)
        )

    def test_mutex_reprovado(self):
        self.inject("std::mutex mutex;")
        _, violations = audit(self.root)

        self.assertTrue(
            any("mutex" in violation for violation in violations)
        )

    def test_lock_reprovado(self):
        self.inject("mutex.lock();")
        _, violations = audit(self.root)

        self.assertTrue(
            any("lock" in violation for violation in violations)
        )

    def test_printf_reprovado(self):
        self.inject('printf("audio");')
        _, violations = audit(self.root)

        self.assertTrue(
            any("printf" in violation for violation in violations)
        )

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

        self.assertTrue(
            any("renderSine" in violation for violation in violations)
        )

    def test_arquivo_ausente_reprovado(self):
        path = self.root / "src/audio/limiter.h"
        path.unlink()

        _, violations = audit(self.root)

        self.assertTrue(
            any("arquivo não encontrado" in violation
                for violation in violations)
        )

    def test_cout_reprovado(self):
        self.inject('std::cout << "audio";')
        _, violations = audit(self.root)

        self.assertTrue(
            any("cout" in violation for violation in violations)
        )

    def test_condition_variable_reprovada(self):
        self.inject("std::condition_variable cv;")
        _, violations = audit(self.root)

        self.assertTrue(
            any("condition_variable" in violation
                for violation in violations)
        )

    def test_sleep_reprovado(self):
        self.inject(
            "std::this_thread::sleep_for(std::chrono::milliseconds(1));"
        )
        _, violations = audit(self.root)

        self.assertTrue(
            any("sleep_for" in violation
                for violation in violations)
        )

    def test_comentario_na_funcao_ignorado(self):
        self.inject("// malloc(128);")
        _, violations = audit(self.root)

        self.assertEqual(violations, [])

    def test_alocacao_fora_do_callback_permitida(self):
        path = self.root / "src/audio/audio_engine.cpp"
        source = path.read_text(encoding="utf-8")

        self.assertIn("new ma_device()", source)

        checked, violations = audit(self.root)

        self.assertEqual(
            checked, sum(map(len, TARGETS.values()))
        )
        self.assertEqual(violations, [])

    def test_inicializacao_estatica_na_wavetable_reprovada(self):
        path = self.root / "src/audio/wavetable.h"
        source = path.read_text(encoding="utf-8")

        marker = "inline float lookupSine(std::uint32_t phase) {"
        self.assertIn(marker, source)

        forbidden_code = """
            static const int valor = []() {
                return 42;
            }();
        """

        source = source.replace(
            marker,
            marker + forbidden_code,
            1,
        )

        path.write_text(source, encoding="utf-8")

        _, violations = audit(self.root)

        self.assertTrue(
            any(
                "lookupSine" in violation
                and "static" in violation
                for violation in violations
            ),
            "A auditoria deveria detectar inicializacao estatica local"
        )

    def test_inicializacao_estatica_por_funcao_reprovada(self):
            path = self.root / "src/audio/wavetable.h"
            source = path.read_text(encoding="utf-8")

            marker = "inline float lookupSine(std::uint32_t phase) {"
            self.assertIn(marker, source)

            forbidden_code = """
                static int valor = calcularValor();
            """

            source = source.replace(
                marker,
                marker + forbidden_code,
                1,
            )

            path.write_text(source, encoding="utf-8")

            _, violations = audit(self.root)

            self.assertTrue(
                any(
                    "lookupSine" in violation
                    and "static" in violation
                    for violation in violations
                ),
                "A auditoria deveria detectar static inicializado por funcao"
            )


if __name__ == "__main__":
    unittest.main()
