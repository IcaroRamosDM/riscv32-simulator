#!/usr/bin/env python3
"""End-to-end checks using the installed RV32I compiler and GNU debug tools."""
import argparse
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
BINARY = None


def command(args, **kwargs):
    return subprocess.run([str(arg) for arg in args], cwd=ROOT, text=True,
                          capture_output=True, timeout=30, **kwargs)


class GuestTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        for name in ("gcc", "objcopy", "objdump", "addr2line", "nm"):
            if shutil.which("riscv64-unknown-elf-" + name) is None:
                raise RuntimeError("Install gcc-riscv64-unknown-elf and binutils-riscv64-unknown-elf")
        cls.workspace = tempfile.TemporaryDirectory(prefix="rv32-guest-")
        cls.addClassCleanup(cls.workspace.cleanup)
        cls.root = Path(cls.workspace.name)
        cls.learning = cls.compile("learning", ROOT / "demos/c/learning.c")
        cls.optimized = cls.compile("optimized", ROOT / "demos/c/learning.c", opt="2")
        cls.arithmetic = cls.compile("arithmetic", ROOT / "demos/c/arithmetic.c")

    @classmethod
    def compile(cls, name, *sources, opt="0", ram="64KiB", stack="4KiB"):
        output = cls.root / name
        result = command([sys.executable, ROOT / "tools/build_guest.py", "--source", *sources,
                          "--output", output, "--opt", opt, "--ram", ram, "--stack", stack])
        if result.returncode:
            raise AssertionError(result.stdout + result.stderr)
        return output

    def invoke(self, *args, commands=None):
        return command([BINARY, *args, "--no-color"], input=commands)

    def run_elf(self, stem, *extra):
        return self.invoke("--elf", str(stem) + ".elf", "--run", *extra)

    def symbols(self, stem):
        result = command(["riscv64-unknown-elf-nm", "-n", str(stem) + ".elf"])
        self.assertEqual(result.returncode, 0, result.stderr)
        return {name: int(address, 16) for address, _, name in
                re.findall(r"^([0-9a-fA-F]+)\s+(\w)\s+(\S+)$", result.stdout, re.M)}

    def test_c_variables_decisions_loops_functions_arrays_pointers(self):
        for stem in (self.learning, self.optimized):
            with self.subTest(stem=stem):
                result = self.run_elf(stem)
                self.assertEqual(result.returncode, 0, result.stderr + result.stdout)
                self.assertIn("program exited", result.stdout)
                self.assertIn("exit=0", result.stdout)

    def test_libgcc_32_and_64_bit_arithmetic(self):
        result = self.run_elf(self.arithmetic)
        self.assertEqual(result.returncode, 0, result.stderr + result.stdout)
        names = self.symbols(self.arithmetic)
        self.assertIn("__mulsi3", names)
        self.assertIn("__divsi3", names)
        self.assertIn("__udivdi3", names)

    def test_elf_and_binary_execute_identically(self):
        elf = self.run_elf(self.learning)
        raw = self.invoke("--bin", str(self.learning) + ".bin",
                          "--load-address", "0x1000", "--entry", "0x1000", "--run")
        self.assertEqual(raw.returncode, 0, raw.stderr)
        self.assertEqual(raw.stdout, elf.stdout)
        for missing in ([], ["--entry", "0x1000"], ["--load-address", "0x1000"]):
            result = self.invoke("--bin", str(self.learning) + ".bin", *missing)
            self.assertEqual(result.returncode, 2)
            self.assertIn("requires both", result.stderr)

    def test_cli_input_formats_and_address_overrides(self):
        for extra in (["--entry", "0x1000"], ["--load-address", "0x1000"],
                      ["--bin", str(self.learning) + ".bin"], ["--program", "demos/sum.words"]):
            result = self.invoke("--elf", str(self.learning) + ".elf", *extra)
            self.assertEqual(result.returncode, 2, result.stdout)

    def test_globals_bss_and_reset(self):
        names = self.symbols(self.learning)
        result_address, calls_address = names["result"], names["calls"]
        commands = (f"mem {result_address} 4\nmem {calls_address} 4\nrun\n"
                    f"mem {result_address} 4\nmem {calls_address} 4\nreset\n"
                    f"mem {result_address} 4\nmem {calls_address} 4\nrun\nquit\n")
        result = self.invoke("--elf", str(self.learning) + ".elf", commands=commands)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout.count("exit=0"), 2)
        self.assertIn(f"0x{result_address:08X}: 48 00 00 00", result.stdout)
        self.assertIn(f"0x{calls_address:08X}: 02 00 00 00", result.stdout)
        self.assertEqual(result.stdout.count(f"0x{calls_address:08X}: 00 00 00 00"), 2)

    def test_configured_ram_stack_and_linker_fit(self):
        result = self.run_elf(self.learning, "--ram", "32KiB")
        self.assertEqual(result.returncode, 2)
        self.assertIn("does not fit", result.stderr)
        stem = self.compile("large-ram", ROOT / "demos/c/learning.c", ram="1MiB", stack="8KiB")
        result = self.run_elf(stem, "--ram", "1MiB")
        self.assertEqual(result.returncode, 0, result.stderr)
        symbols = self.symbols(stem)
        self.assertEqual(symbols["__stack_top"], 1024 * 1024)
        self.assertEqual(symbols["__stack_bottom"], 1024 * 1024 - 8192)
        source = self.root / "too_large.c"
        source.write_text("char global[60000];\nint main(void) { return global[0]; }\n")
        failed = command([sys.executable, ROOT / "tools/build_guest.py", "--source", source,
                          "--output", self.root / "too-large"])
        self.assertEqual(failed.returncode, 2)
        self.assertIn("overlaps", failed.stderr)

    def test_source_mapping_agrees_with_gnu_addr2line(self):
        checked = 0
        for stem in (self.learning, self.optimized):
            disassembly = command(["riscv64-unknown-elf-objdump", "-d", str(stem) + ".elf"])
            self.assertEqual(disassembly.returncode, 0)
            addresses = re.findall(r"^\s*([0-9a-f]+):\s+[0-9a-f]{8}\s", disassembly.stdout, re.M)
            expected = command(["riscv64-unknown-elf-addr2line", "-e", str(stem) + ".elf",
                                *["0x" + address for address in addresses]])
            self.assertEqual(expected.returncode, 0, expected.stderr)
            rows = expected.stdout.splitlines()
            self.assertEqual(len(rows), len(addresses))
            result = self.invoke("--elf", str(stem) + ".elf",
                                 commands="".join("where 0x" + a + "\n" for a in addresses) + "quit\n")
            self.assertEqual(result.returncode, 0, result.stderr)
            actual = re.findall(r"Source: (.+)", result.stdout)
            self.assertEqual(len(actual), len(addresses))
            for address, reference, observed in zip(addresses, rows, actual):
                # GNU may append a discriminator after the line number.
                match = re.match(r"(.+):(\d+)(?: \(discriminator \d+\))?$", reference)
                if match and int(match[2]) > 0:
                    location = re.fullmatch(r"(.+):(\d+):(\d+)", observed)
                    self.assertIsNotNone(location, (address, reference, observed))
                    self.assertEqual((location[1], location[2]), match.groups(), address)
                    checked += 1
                else:
                    self.assertEqual(observed, "<no instruction-to-source mapping>", address)
        self.assertGreater(checked, 100)

    def test_assembly_steps_keep_the_same_source_label(self):
        result = self.run_elf(self.learning, "--trace")
        self.assertEqual(result.returncode, 0, result.stderr)
        instructions = re.findall(r"^0x[0-9A-F]{8}  ", result.stdout, re.M)
        locations = re.findall(r"^Source: ", result.stdout, re.M)
        self.assertGreater(len(instructions), len(locations))
        self.assertRegex(result.stdout, r"learning.c:\d+:\d+")
        self.assertIn("[changed]", result.stdout)
        self.assertIn("runtime/start.S:", result.stdout)

    def test_symbols_disassembly_and_nonexecutable_source_lines(self):
        names = self.symbols(self.learning)
        result = self.invoke("--elf", str(self.learning) + ".elf",
                             commands=f"sources\nsymbols main\nsymbols result\ndisasm {names['main']} 4\nquit\n")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("function  main", result.stdout)
        self.assertIn("symbol  result", result.stdout)
        self.assertRegex(result.stdout, r"; .+learning.c:\d+")
        match = re.search(r"(\d+)  ([^\n]*learning.c)", result.stdout)
        self.assertIsNotNone(match)
        result = self.invoke("--elf", str(self.learning) + ".elf",
                             commands=f"source 3 {match[1]}\nsource 1 18446744073709551615\nhelp source\nhelp runtime\nquit\n")
        self.assertIn("[no mapped instruction]", result.stdout)
        self.assertIn("volatile uint32_t result;", result.stdout)
        self.assertIn("Use source [positive LINE] [FILE_INDEX].", result.stdout)
        self.assertIn("one-to-one", result.stdout)

    def test_stripped_elf_and_raw_binary_have_explicit_missing_metadata(self):
        stripped = self.root / "stripped.elf"
        result = command(["riscv64-unknown-elf-objcopy", "--strip-all",
                          str(self.learning) + ".elf", stripped])
        self.assertEqual(result.returncode, 0, result.stderr)
        for args in (["--elf", stripped], ["--bin", str(self.learning) + ".bin",
                                           "--load-address", "0x1000", "--entry", "0x1000"]):
            result = self.invoke(*args, commands="where\nsources\nsymbols\nrun\nquit\n")
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn("<no instruction-to-source mapping>", result.stdout)
            self.assertIn("No source files", result.stdout)
            self.assertIn("No symbols", result.stdout)
            self.assertIn("exit=0", result.stdout)

    def test_missing_source_file_preserves_mapping(self):
        source = self.root / "disappearing.c"
        source.write_text("int main(void)\n{\n  return 0;\n}\n")
        stem = self.compile("missing-source", source)
        main = self.symbols(stem)["main"]
        source.unlink()
        result = self.invoke("--elf", str(stem) + ".elf", commands=f"where {main}\nrun\nquit\n")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("Source file unavailable:", result.stdout)
        self.assertIn("Address mapping is still available", result.stdout)
        self.assertIn("exit=0", result.stdout)

    def test_multiple_sources_paths_with_spaces_and_runtime_memory(self):
        source = self.root / "memory operations.c"
        helper = self.root / "helper.c"
        helper.write_text("int helper(int n) { return n ? n + helper(n - 1) : 0; }\n")
        source.write_text("""
#include "rv32_runtime.h"
_Static_assert(sizeof(void *) == 4 && sizeof(long) == 4 && sizeof(int) == 4, "ILP32 required");
int helper(int);
unsigned char global[16];
struct Block { unsigned char bytes[40]; };
static struct Block copy(struct Block value) { return value; }
int main(void)
{
  unsigned char input[8] = {1,2,3,4,5,6,7,8};
  struct Block block = {{11,22,33}};
  struct Block other = copy(block);
  if (global[0] || global[15] || other.bytes[2] != 33) return 1;
  memcpy(global, input, 8);
  memmove(global + 2, global, 8);
  if (global[2] != 1 || global[9] != 8) return 2;
  memmove(global, global + 2, 8);
  if (memcmp(global, input, 8) != 0) return 3;
  memset(global + 8, 0x1ab, 8);
  if (global[8] != 0xab || global[15] != 0xab) return 4;
  return helper(6) == 21 ? 0 : 5;
}
""")
        for opt in ("0", "2"):
            stem = self.compile("output space,comma/memory-ops-" + opt, source, helper, opt=opt)
            result = self.run_elf(stem)
            self.assertEqual(result.returncode, 0, result.stderr + result.stdout)

    def test_failed_compilation_preserves_previous_outputs_and_sources(self):
        source = self.root / "invalid.c"
        source.write_text("int main(void) { return ???; }\n")
        before = {suffix: Path(str(self.learning) + suffix).read_bytes()
                  for suffix in (".elf", ".bin", ".asm", ".map", ".ld")}
        result = command([sys.executable, ROOT / "tools/build_guest.py", "--source", source,
                          "--output", self.learning])
        self.assertEqual(result.returncode, 2)
        self.assertIn("invalid.c:1:", result.stderr)
        for suffix, data in before.items():
            self.assertEqual(Path(str(self.learning) + suffix).read_bytes(), data)
        self.assertEqual(source.read_text(), "int main(void) { return ???; }\n")

    def test_incompatible_elf_flags_architecture_and_debug_rejected(self):
        original = Path(str(self.learning) + ".elf").read_bytes()
        for name, data in (("compressed", bytearray(original)), ("multiply", bytearray(original)),
                           ("debug", bytearray(original))):
            if name == "compressed":
                struct.pack_into("<I", data, 36, 1)
            elif name == "multiply":
                index = data.index(b"rv32i2p1")
                data[index + 4] = ord("m")
            else:
                sections = struct.unpack_from("<I", data, 32)[0]
                count, strings = struct.unpack_from("<HH", data, 48)
                names_offset = struct.unpack_from("<I", data, sections + strings * 40 + 16)[0]
                found = False
                for index in range(count):
                    offset = sections + index * 40
                    name_offset = names_offset + struct.unpack_from("<I", data, offset)[0]
                    end = data.index(0, name_offset)
                    if data[name_offset:end] == b".debug_line":
                        payload = struct.unpack_from("<I", data, offset + 16)[0]
                        struct.pack_into("<I", data, payload, 0xFFFFFFF0)
                        found = True
                        break
                self.assertTrue(found)
            path = self.root / (name + ".elf")
            path.write_bytes(data)
            result = self.invoke("--elf", path, "--run")
            self.assertEqual(result.returncode, 2, (name, result.stdout, result.stderr))

    def test_unsupported_runtime_features_fail_during_link(self):
        for name, text in (
                ("tls", "_Thread_local int value = 7; int main(void) { return value; }"),
                ("constructor", "int x; void init(void) __attribute__((constructor)); "
                                "void init(void) { x = 9; } int main(void) { return x; }")):
            source = self.root / (name + ".c")
            source.write_text(text + "\n")
            result = command([sys.executable, ROOT / "tools/build_guest.py", "--source", source,
                              "--output", self.root / name])
            self.assertEqual(result.returncode, 2, result.stdout)
            self.assertIn("unsupported runtime", result.stderr)

    def test_assembler_and_abi_stack_alignment(self):
        source = self.root / "assembly_caller.c"
        assembly = self.root / "increment.S"
        source.write_text("int increment(int); int main(void) { return increment(41) == 42 ? 0 : 1; }\n")
        assembly.write_text(".text\n.globl increment\n.type increment,@function\nincrement:\n"
                            "addi a0,a0,1\njalr zero,0(ra)\n.size increment,.-increment\n")
        stem = self.compile("assembly-call", source, assembly)
        result = self.run_elf(stem, "--trace")
        self.assertEqual(result.returncode, 0, result.stderr + result.stdout)
        stack_values = re.findall(r"x2: 0x[0-9A-F]+ -> 0x([0-9A-F]+)", result.stdout)
        self.assertGreater(len(stack_values), 1)
        self.assertTrue(all(int(value, 16) % 16 == 0 for value in stack_values))
        self.assertIn("increment.S:", result.stdout)

    def test_build_argument_validation(self):
        for args in (["--ram", "64KB"], ["--ram", "8192B"], ["--stack", "3B"],
                     ["--ram", "512MiB"], ["--cross-prefix", "nonexistent-rv32-"]):
            result = command([sys.executable, ROOT / "tools/build_guest.py", "--source",
                              ROOT / "demos/c/learning.c", "--output", self.root / "bad", *args])
            self.assertEqual(result.returncode, 2, result.stdout)
        self.assertFalse((self.root / "bad.elf").exists())


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--binary", type=Path, required=True)
    options = parser.parse_args()
    BINARY = options.binary.resolve()
    unittest.main(argv=[sys.argv[0]])
