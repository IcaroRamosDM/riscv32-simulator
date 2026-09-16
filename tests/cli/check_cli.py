#!/usr/bin/env python3
"""Process-level checks for the teaching monitor; requires only Python's standard library."""
import argparse
import os
import signal
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BINARY = None


class MonitorTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)

    def program(self, text):
        path = Path(self.temp.name) / "program.words"
        path.write_bytes(text if isinstance(text, bytes) else text.encode())
        return str(path)

    def invoke(self, *args, commands=None):
        return subprocess.run([str(BINARY), *args], cwd=ROOT, input=commands, text=True,
                              capture_output=True, timeout=10, check=False)

    def test_help(self):
        result = self.invoke("--help")
        self.assertEqual(result.returncode, 0)
        self.assertIn("step [N]", result.stdout)
        self.assertIn("--max-steps", result.stdout)

    def test_demos(self):
        for demo, pc in [("arithmetic", "00000018"), ("sum", "0000002C"), ("call", "00000018")]:
            with self.subTest(demo=demo):
                result = self.invoke("--program", f"demos/{demo}.words", "--run", "--trace")
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertIn("program exited", result.stdout)
                self.assertIn(f"PC=0x{pc}; exit=0", result.stdout)
                self.assertIn("[changed]", result.stdout)
                self.assertNotIn("\x1b", result.stdout)

    def test_sum_inspection_and_reset(self):
        result = self.invoke("--program", "demos/sum.words",
                             commands="step\nregs\nrun\nmem 0x100 4\nreset\nmem 0x100 4\nregs\nquit\n")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("x5  t0    0x00000001", result.stdout)
        self.assertIn("0x00000100: 0F 00 00 00", result.stdout)
        self.assertIn("0x00000100: 00 00 00 00", result.stdout)
        self.assertIn("PC=0x00000000  retired=0", result.stdout)

    def test_call_and_stack(self):
        result = self.invoke("--program", "demos/call.words",
                             commands="run\nregs\nmem 0xFFF8 8\nquit\n")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("x5  t0    0x0000000E", result.stdout)
        self.assertIn("x2  sp    0x00010000", result.stdout)
        self.assertIn("0x0000FFF8: 07 00 00 00 0C 00 00 00", result.stdout)

    def test_disassembly_and_context_help(self):
        result = self.invoke("--program", "demos/arithmetic.words",
                             commands="disasm 0 2\nhelp formats\nhelp ADDI\nhelp nonexistent\nquit\n")
        self.assertEqual(result.returncode, 0)
        self.assertIn("00C00293  addi x5, x0, 12", result.stdout)
        self.assertIn("constant comes from the instruction", result.stdout)
        self.assertIn("Unknown help topic", result.stdout)
        self.assertIn("addi", result.stdout)

    def test_color_and_manual_stop(self):
        result = self.invoke("--program", "demos/arithmetic.words", "--color",
                             commands="stop\nstep\nregs\nquit\n")
        self.assertEqual(result.returncode, 0)
        self.assertIn("CPU stopped", result.stdout)
        self.assertIn("\x1b[36;1m", result.stdout)
        self.assertIn("retired=1", result.stdout)

    def test_limits(self):
        path = self.program("0000006F\n")
        result = self.invoke("--program", path, "--run", "--max-steps", "0x5")
        self.assertEqual(result.returncode, 124)
        self.assertIn("attempts=5; retired=5; PC=0x00000000", result.stdout)

    def test_traps(self):
        for word, message in [("00000000", "illegal"), ("00100073", "breakpoint"),
                              ("00000073", "unsupported ECALL service a7=0"),
                              ("00202183", "load address misaligned")]:
            with self.subTest(word=word):
                result = self.invoke("--program", self.program(word), "--run")
                self.assertEqual(result.returncode, 1)
                self.assertIn(message, result.stdout)
                self.assertIn("retired=0; PC=0x00000000", result.stdout)

    def test_exit_code_and_relocation(self):
        path = self.program("12C00513\n05D00893\n00000073")
        result = self.invoke("--program", path, "--run", "--load-address", "0x100")
        self.assertEqual(result.returncode, 44)
        self.assertIn("PC=0x00000108; exit=300", result.stdout)
        result = self.invoke("--program", path, "--run", "--load-address", "0x100", "--entry", "0x104")
        self.assertEqual(result.returncode, 0)

    def test_rejected_options(self):
        for args in [[], ["--wat"], ["--program"], ["--program", "/does/not/exist"],
                     ["--max-steps", "0"], ["--max-steps", "-1"], ["--max-steps", "1e3"],
                     ["--max-steps", "18446744073709551616"], ["--max-steps", "0x"],
                     ["--load-address", "4294967296"]]:
            with self.subTest(args=args):
                result = self.invoke(*args)
                self.assertEqual(result.returncode, 2)

    def test_rejected_files_and_addresses(self):
        for text in ["", "# empty\n", "100000000\n", "13 13\n", "GG\n", "13\x0073\n"]:
            with self.subTest(text=text):
                result = self.invoke("--program", self.program(text), "--run")
                self.assertEqual(result.returncode, 2)
        path = self.program("00000013")
        for args in [["--load-address", "2"], ["--load-address", "65536"], ["--entry", "4"]]:
            result = self.invoke("--program", path, *args)
            self.assertEqual(result.returncode, 2)

    def test_invalid_commands(self):
        result = self.invoke("--program", "demos/arithmetic.words",
                             commands="step 0\nrun -1\nmem 65535 4\ndisasm 2 1\nregs extra\n"
                                      + "x" * 700 + "\nquit\n")
        self.assertEqual(result.returncode, 0)
        self.assertIn("Instruction limit must be a positive integer", result.stdout)
        self.assertIn("Invalid inspection range", result.stdout)
        self.assertIn("Command too long", result.stdout)
        self.assertNotIn("Stopped:", result.stdout)

    def test_ram_units_and_sizes(self):
        path = self.program("00000013")
        for spelling, size in [("4B", 4), ("12B", 12), ("1024B", 1024),
                               ("64KiB", 65536), ("96KiB", 98304),
                               ("0x40KiB", 65536), ("1MiB", 1048576)]:
            with self.subTest(spelling=spelling):
                result = self.invoke("--program", path, "--ram", spelling,
                                     commands="regs\nstep\nquit\n")
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertIn(f"RAM={size} bytes", result.stdout)
                self.assertIn("attempts=1; retired=1", result.stdout)

    def test_invalid_ram_sizes(self):
        path = self.program("00000013")
        for size in ["", "64", "64KB", "64kib", "-1KiB", "+4B", "0B", "1B", "6B",
                     "257MiB", "268435460B", "18446744073709551616MiB", "1.5MiB",
                     " 64KiB", "64KiB ", "0xKiB", "0x100000000B"]:
            with self.subTest(size=size):
                result = self.invoke("--program", path, "--ram", size, "--run")
                self.assertEqual(result.returncode, 2)
                self.assertIn("Invalid RAM size", result.stderr)
        result = self.invoke("--program", path, "--ram")
        self.assertEqual(result.returncode, 2)

    def test_program_fit_and_data_bounds(self):
        result = self.invoke("--program", "demos/sum.words", "--ram", "32B", "--run")
        self.assertEqual(result.returncode, 2)
        self.assertIn("program does not fit in RAM", result.stderr)
        result = self.invoke("--program", "demos/sum.words", "--ram", "256B", "--run")
        self.assertEqual(result.returncode, 1)
        self.assertIn("store access fault", result.stdout)
        result = self.invoke("--program", "demos/sum.words", "--ram", "1KiB", "--run")
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_program_above_old_ram_limit(self):
        args = ["--program", "demos/arithmetic.words", "--load-address", "0x10000"]
        result = self.invoke(*args, "--ram", "128KiB",
                             commands="disasm 0x10000 1\nmem 0x10000 4\nrun\nquit\n")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("00C00293  addi x5, x0, 12", result.stdout)
        self.assertIn("0x00010000: 93 02 C0 00", result.stdout)
        self.assertIn("PC=0x00010018; exit=0", result.stdout)
        result = self.invoke(*args, "--ram", "64KiB", "--run")
        self.assertEqual(result.returncode, 2)

    def test_custom_ram_inspection_limits(self):
        path = self.program("00000013")
        result = self.invoke("--program", path, "--ram", "1KiB",
                             commands="mem 1020 4\nmem 1023 2\ndisasm 1020 1\n"
                                      "disasm 1022 1\ndisasm 1024 1\nquit\n")
        self.assertEqual(result.returncode, 0)
        self.assertIn("0x000003FC: 00 00 00 00", result.stdout)
        self.assertIn("0x000003FC  00000000  .word 0x00000000", result.stdout)
        self.assertEqual(result.stdout.count("Invalid inspection range"), 3)

    def test_reset_keeps_selected_capacity(self):
        result = self.invoke("--program", "demos/sum.words", "--ram", "1MiB",
                             commands="run\nmem 0x100 4\nreset\nmem 0x100 4\nregs\nquit\n")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("0x00000100: 0F 00 00 00", result.stdout)
        self.assertIn("0x00000100: 00 00 00 00", result.stdout)
        self.assertIn("RAM=1048576 bytes", result.stdout)
        self.assertIn("PC=0x00000000  retired=0", result.stdout)

    @unittest.skipUnless(os.name == "posix", "POSIX signal check")
    def test_ctrl_c(self):
        path = self.program("0000006F\n")
        process = subprocess.Popen([str(BINARY), "--program", path, "--run", "--trace",
                                    "--max-steps", "18446744073709551615"], cwd=ROOT,
                                   stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        try:
            # Output proves execution and handler installation before delivering SIGINT.
            line = process.stdout.readline()
            if line.startswith("Source:"):
                line = process.stdout.readline()
            self.assertIn("jal x0, 0", line)
            process.send_signal(signal.SIGINT)
            output, errors = process.communicate(timeout=10)
            self.assertEqual(process.returncode, 130, errors)
            self.assertIn("Stopped: stopped", output)
        finally:
            if process.poll() is None:
                process.kill()
                process.communicate()


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--binary", required=True, type=Path)
    args = parser.parse_args()
    BINARY = args.binary.resolve()
    unittest.main(argv=["check_cli"], verbosity=1)
