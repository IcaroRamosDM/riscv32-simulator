#!/usr/bin/env python3
"""Compare PC/registers each step, a RAM window each step, and all RAM at completion."""
import argparse
import random
import struct
import subprocess
from pathlib import Path

try:
    import unicorn
    from unicorn import Uc, UC_ARCH_RISCV, UC_MODE_RISCV32
    from unicorn.riscv_const import UC_RISCV_REG_X0, UC_RISCV_REG_PC
except ImportError as exc:
    raise SystemExit("Install tests/reference/requirements.txt in a virtual environment first.") from exc

RAM_SIZE = 65536
FRAME_SIZE = 33 * 4 + 1024
ROOT = Path(__file__).resolve().parents[2]


def compare(driver, name, memory, registers, pc, steps):
    request = struct.pack("<34I", pc, steps, *registers) + memory
    result = subprocess.run([str(driver)], input=request, capture_output=True, timeout=30, check=False)
    if result.returncode:
        raise AssertionError(f"{name}: driver failed: {result.stderr.decode()}")
    assert len(result.stdout) == steps * FRAME_SIZE + RAM_SIZE, name
    reference = Uc(UC_ARCH_RISCV, UC_MODE_RISCV32)
    reference.mem_map(0, RAM_SIZE)
    reference.mem_write(0, bytes(memory))
    for reg, value in enumerate(registers):
        reference.reg_write(UC_RISCV_REG_X0 + reg, value)
    reference.reg_write(UC_RISCV_REG_PC, pc)
    for step in range(steps):
        pc = reference.reg_read(UC_RISCV_REG_PC)
        reference.emu_start(pc, 0xFFFFFFFF, count=1)
        offset = step * FRAME_SIZE
        expected = [reference.reg_read(UC_RISCV_REG_PC)]
        expected.extend(reference.reg_read(UC_RISCV_REG_X0 + reg) for reg in range(32))
        actual = struct.unpack_from("<33I", result.stdout, offset)
        if tuple(expected) != actual:
            mismatches = [(index, hex(got), hex(want)) for index, (got, want)
                          in enumerate(zip(actual, expected)) if got != want]
            raise AssertionError(f"{name} step {step} PC={pc:#x}: (0=PC, 1=x0) {mismatches}")
        actual_ram = result.stdout[offset + 132:offset + FRAME_SIZE]
        assert actual_ram == reference.mem_read(0x8000, 1024), f"{name} step {step}: RAM window"
    assert result.stdout[steps * FRAME_SIZE:] == reference.mem_read(0, RAM_SIZE), f"{name}: final RAM"
    return steps


def immediate(opcode, funct3, rd, rs1, imm):
    return ((imm & 4095) << 20) | (rs1 << 15) | (funct3 << 12) | (rd << 7) | opcode


def register(funct3, funct7, rd, rs1, rs2):
    return funct7 << 25 | rs2 << 20 | rs1 << 15 | funct3 << 12 | rd << 7 | 0x33


def store(funct3, rs1, rs2, imm):
    return ((imm & 0xFE0) << 20) | rs2 << 20 | rs1 << 15 | funct3 << 12 | (imm & 31) << 7 | 0x23


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--driver", required=True, type=Path)
    args = parser.parse_args()
    driver = args.driver.resolve()
    rng = random.Random(0x52563332)
    # One independently specified encoding for each retiring RV32I operation.
    words = [
        0x003100B3, 0x403100B3, 0x003110B3, 0x003120B3, 0x003130B3,
        0x003140B3, 0x003150B3, 0x403150B3, 0x003160B3, 0x003170B3,
        0xFFF10093, 0x80012093, 0x7FF13093, 0xFFF14093, 0x00116093,
        0x0FF17093, 0x01F11093, 0x01F15093, 0x41F15093,
        0xFFFFF0B7, 0x12345097, 0x008000EF, 0xFFC100E7,
        0x00310463, 0x00311463, 0x00314463, 0x00315463, 0x00316463, 0x00317463,
        0xFFC10083, 0xFFC11083, 0xFFC12083, 0xFFC14083, 0xFFC15083,
        0xFE310E23, 0xFE311E23, 0xFE312E23, 0x0FF0000F,
    ]
    boundaries = [0, 1, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF]
    steps = cases = 0
    for word in words:
        for variant, boundary in enumerate(boundaries):
            memory = bytearray(rng.randbytes(RAM_SIZE))
            struct.pack_into("<I", memory, 0x1000, word)
            regs = [0] + [rng.getrandbits(32) for _ in range(31)]
            regs[2] = boundary
            regs[3] = boundaries[-variant - 1]
            if word & 0x7F in (3, 0x23):
                regs[2] = 0x8004
            if word & 0x7F == 0x67:
                regs[2] = 0x1009  # JALR clears bit zero after adding -4.
            steps += compare(driver, f"word={word:08x}/variant={variant}", memory, regs, 0x1000, 1)
            cases += 1
    for seed in range(4):
        randomizer = random.Random(seed)
        memory = bytearray(randomizer.randbytes(RAM_SIZE))
        regs = [0] + [randomizer.getrandbits(32) for _ in range(31)]
        regs[31] = 0x8000  # Reserved data base in generated sequences.
        for index in range(1000):
            rd, rs1, rs2 = (randomizer.randrange(31) for _ in range(3))
            family = randomizer.randrange(5)
            if family == 0:
                f3, f7 = randomizer.choice([(x, 0) for x in range(8)] + [(0, 32), (5, 32)])
                word = register(f3, f7, rd, rs1, rs2)
            elif family == 1:
                word = immediate(0x13, randomizer.choice([0, 2, 3, 4, 6, 7]),
                                 rd, rs1, randomizer.randrange(-2048, 2048))
            elif family == 2:
                f3, f7 = randomizer.choice([(1, 0), (5, 0), (5, 32)])
                word = immediate(0x13, f3, rd, rs1, f7 * 32 + randomizer.randrange(32))
            elif family == 3:
                word = randomizer.getrandbits(20) << 12 | rd << 7 | randomizer.choice([0x17, 0x37])
            else:
                width = randomizer.choice([1, 2, 4])
                offset = randomizer.randrange(1024 // width) * width
                f3 = {1: 0, 2: 1, 4: 2}[width]
                if randomizer.randrange(2):
                    word = store(f3, 31, rs2, offset)
                else:
                    f3 += 4 if width < 4 and randomizer.randrange(2) else 0
                    word = immediate(3, f3, rd, 31, offset)
            struct.pack_into("<I", memory, 0x1000 + index * 4, word)
        steps += compare(driver, f"random-sequence-{seed}", memory, regs, 0x1000, 1000)
        cases += 1
    for name, count in [("arithmetic", 6), ("sum", 23), ("call", 13)]:
        memory = bytearray(RAM_SIZE)
        words = [int(line.split("#")[0].strip(), 16) for line in
                 (ROOT / "demos" / f"{name}.words").read_text().splitlines() if line.split("#")[0].strip()]
        for index, word in enumerate(words):
            struct.pack_into("<I", memory, index * 4, word)
        steps += compare(driver, f"demo-{name}", memory, [0] * 32, 0, count)
        cases += 1
    print(f"Unicorn {unicorn.__version__}: {cases} scenarios, {steps} instructions matched.")
    print("Compared 38 retiring RV32I operations; native tests cover traps and strict misalignment.")


if __name__ == "__main__":
    main()
