#!/usr/bin/env python3
"""Build a freestanding RV32I/ILP32 program and its inspection artifacts."""
import argparse
import os
from pathlib import Path
import re
import shutil
import shlex
import struct
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
MAX_RAM = 256 * 1024 * 1024


def size_bytes(text):
    match = re.fullmatch(r"(0[xX][0-9a-fA-F]+|[0-9]+)(B|KiB|MiB)", text)
    if not match:
        raise argparse.ArgumentTypeError("use an integer with B, KiB, or MiB")
    digits, unit = match.groups()
    value = int(digits, 16 if digits.lower().startswith("0x") else 10)
    value *= {"B": 1, "KiB": 1024, "MiB": 1024 * 1024}[unit]
    if value < 16 or value > MAX_RAM or value % 16:
        raise argparse.ArgumentTypeError("guest RAM/stack must be 16-byte aligned and at most 256MiB")
    return value


def run(args, **kwargs):
    return subprocess.run([str(arg) for arg in args], check=True, **kwargs)


def build(args):
    sources = [Path(source).resolve(strict=True) for source in args.source]
    if any(source.suffix not in {".c", ".s", ".S"} or not source.is_file() for source in sources):
        raise ValueError("sources must be C or assembly files (.c, .s, .S)")
    if args.ram <= 0x1000 + args.stack:
        raise ValueError("RAM must contain the 0x1000 origin, program, and reserved stack")
    tools = {name: shutil.which(args.cross_prefix + name)
             for name in ("gcc", "objcopy", "objdump", "readelf")}
    if not all(tools.values()):
        raise ValueError("install gcc-riscv64-unknown-elf and binutils-riscv64-unknown-elf")
    target = ["-march=rv32i", "-mabi=ilp32", "-mno-relax"]
    libgcc = run([tools["gcc"], *target, "-print-libgcc-file-name"],
                 capture_output=True, text=True).stdout.strip()
    if not Path(libgcc).is_file():
        raise ValueError("the toolchain has no libgcc for RV32I/ILP32")
    output = Path(args.output).resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    destinations = [Path(str(output) + suffix) for suffix in (".elf", ".bin", ".asm", ".map", ".ld")]
    if any(path in sources for path in destinations):
        raise ValueError("output would overwrite an input source")
    with tempfile.TemporaryDirectory(prefix="guest-", dir=output.parent) as directory:
        work = Path(directory)
        linker = work / "program.ld"
        template = (ROOT / "runtime/link.ld.in").read_text()
        linker.write_text(template.replace("@RAM_SIZE@", str(args.ram))
                          .replace("@STACK_SIZE@", str(args.stack)))
        elf = work / "program.elf"
        flags = ["-I", ROOT / "runtime/include", "-std=c17", "-Wall", "-Wextra", "-Wpedantic", "-Werror",
                 "-ffreestanding", "-fno-builtin", "-fno-pie", "-fno-stack-protector",
                 "-fno-unwind-tables", "-fno-asynchronous-unwind-tables",
                 "-msmall-data-limit=0", "-g", "-gdwarf-4", "-O" + args.opt]
        run([tools["gcc"], *target, *flags, "-nostdlib", "-nostartfiles", "-static",
             ROOT / "runtime/start.S", *sources, ROOT / "runtime/memory.c",
             "-Wl,--no-relax", "-Wl,--build-id=none", "-T", linker,
             "-Xlinker", "-Map", "-Xlinker", work / "program.map", "-lgcc", "-o", elf])
        header = elf.read_bytes()[:52]
        if len(header) != 52 or header[:7] != b"\x7fELF\x01\x01\x01" or (
                struct.unpack_from("<H", header, 18)[0] != 243 or
                struct.unpack_from("<I", header, 36)[0] != 0):
            raise ValueError("toolchain produced an incompatible ELF header")
        attributes = run([tools["readelf"], "-A", elf], capture_output=True, text=True).stdout
        architectures = re.findall(r'Tag_RISCV_arch: "([^"]+)"', attributes)
        if not architectures or any(arch not in {"rv32i2p0", "rv32i2p1", "rv32i"}
                                    for arch in architectures):
            raise ValueError("linked runtime requires an ISA beyond RV32I: " + repr(architectures))
        run([tools["objcopy"], "-O", "binary", elf, work / "program.bin"])
        with (work / "program.asm").open("w") as listing:
            run([tools["objdump"], "-d", "-S", "-l", "-M", "no-aliases", elf], stdout=listing)
        for destination in destinations:
            os.replace(work / ("program" + destination.suffix), destination)
    print("Target: RV32I / ILP32; optimization: -O" + args.opt)
    print("RAM: " + str(args.ram) + " bytes; stack: " + str(args.stack) + " bytes")
    print("Runtime library: " + libgcc)
    for path in destinations:
        print(path)
    print("Run: " + shlex.join(["./build/debug/riscv32-studio", "--elf", str(output) + ".elf",
                                "--ram", str(args.ram) + "B"]))
    print("Raw binary: --bin FILE --load-address 0x1000 --entry 0x1000"
          + " --ram " + str(args.ram) + "B (no symbols or source mapping)")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", nargs="+", required=True, help="C/assembly source paths")
    parser.add_argument("--output", default="build/guest/program", help="artifact path without extension")
    parser.add_argument("--ram", type=size_bytes, default=size_bytes("64KiB"))
    parser.add_argument("--stack", type=size_bytes, default=size_bytes("4KiB"))
    parser.add_argument("--opt", choices=("0", "1", "2", "3", "s", "g"), default="0")
    parser.add_argument("--cross-prefix", default="riscv64-unknown-elf-")
    args = parser.parse_args()
    try:
        build(args)
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print("Guest build failed: " + str(error), file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
