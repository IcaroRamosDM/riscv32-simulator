# RISC-V Studio

An educational desktop simulator being developed to connect C source code,
RISC-V assembly, registers, and memory through instruction-by-instruction execution.

The project targets RV32I and uses a C17 simulator core. It is currently at the
machine-state and instruction-decoding stage: instruction execution, C compilation, file
loading, and the graphical interface are planned work.

## Implemented

- CPU state with 32 registers, a program counter, an instruction counter, and a
  halted flag.
- CPU reset and checked register access. Reads of `x0` return zero; writes to
  `x0` succeed and discard the value.
- A separate 64 KiB RAM object with reset and checked 8-bit, 16-bit, and 32-bit
  reads and writes, using little-endian byte order and natural alignment.
- Read-only instruction fetch using the program counter, including while halted.
- Raw instruction-field extraction with preservation of the original word.
- ADD recognition using opcode, funct3, and funct7, with unknown words preserved.
- Independent tests for CPU reset, registers, RAM, fetch, fields, and decoding.
- Debug, optimized, and sanitizer build configurations.

See the [development roadmap](docs/ROADMAP.md) and the
[core flowcharts](docs/FLOWCHARTS.md) for the current design and next stages.
The [learning guide](docs/HELP.md) explains formats, register values, and encoding.

## Build and test

The current build requires a Linux environment, GCC with C17 support, GNU Make,
and standard command-line tools including `find`. Development takes place on
Ubuntu under WSL 2.

From the repository root:

```bash
make check
make MODE=release test
```

`make check` runs the tests in debug mode and then with AddressSanitizer and
UndefinedBehaviorSanitizer. The second command runs the optimized tests.

Each configuration should report:

```text
CPU reset tests passed.
CPU fetch tests passed.
CPU register tests passed.
Instruction decode tests passed.
Instruction field tests passed.
Memory tests passed.
Memory word tests passed.
```

The tests cover the implemented APIs; passing them does not establish complete
RV32I instruction-set conformance.

### Build commands

| Command | Action |
| --- | --- |
| `make` | Build tests and the application when its entry point exists. |
| `make test` | Build and run tests in the selected mode; debug is the default. |
| `make debug` | Build with `-O0 -g3`. |
| `make release` | Build with `-O2 -g`. |
| `make MODE=release test` | Run optimized tests. |
| `make sanitize` | Run tests with address and undefined-behavior sanitizers. |
| `make check` | Run debug and sanitizer tests. |
| `make app` | Build the application once `src/main.c` exists. |
| `make run ARGS="..."` | Build and run the application with arguments. |
| `make clean` | Remove generated files under `build/`. |
| `make help` | Show the complete build help. |

There is no application entry point yet, so `make app` and `make run` currently
report that `src/main.c` is missing.

Artifacts are separated under `build/debug/`, `build/release/`, and
`build/sanitize/`. New core sources under `src/` and tests matching
`tests/**/test_*.c` are discovered automatically. Each test source supplies its
own `main`; shared test helpers belong under `tests/support/`.

The build tracks header dependencies, compiler settings, and source-list changes.
Test assertions remain enabled in every mode. The compiler can be overridden
with `CC`; additional options use `CPPFLAGS`, `CFLAGS`, `LDFLAGS`, and `LDLIBS`.

## Core API

### CPU

`Cpu` stores architectural state separately from RAM. `cpu_reset` zeroes all
registers and counters and clears the halted flag.

`cpu_read_register` and `cpu_write_register` return `false` for null required
pointers or indices outside `0..31`. A rejected access leaves the CPU and any
provided output value unchanged. A write to `x0` is a valid operation that
discards its input.

Use these functions for simulated register access. The exposed structure allows
state inspection, but direct field writes bypass the access rules.

### RAM

`Memory` contains 65,536 bytes mapped from `0x00000000` through `0x0000FFFF`.
The 64 KiB capacity keeps the initial machine model and test snapshots small.
It is a simulator configuration choice. RV32I uses a 32-bit byte-addressed
space, which can represent 4 GiB of addresses; that does not require 4 GiB
of installed RAM. Addresses outside the configured RAM currently fail access
validation. Configurable RAM capacity is planned with the program loader,
including explicit units, allocation limits, program-fit checks, and a saved
project setting. The implementation currently remains fixed at 64 KiB.

`memory_reset` clears every byte. The `memory_read_u8/u16/u32` and
`memory_write_u8/u16/u32` functions return `false` for null required pointers,
misaligned addresses, or accesses that do not fit entirely inside RAM. Validation
happens before indexing the array, and rejected accesses leave RAM and any
provided output value unchanged.

Both reset functions require a valid pointer to their respective object.
Access functions accept valid object pointers or null; they cannot validate
arbitrary dangling pointers. Read outputs should point to separate writable
storage.

Multi-byte accesses use little-endian byte order and require natural alignment:
16-bit accesses start at multiples of 2, and 32-bit accesses start at multiples
of 4. Byte accesses have no additional alignment requirement. For example,
writing `0xFEDCBA98` stores `98 BA DC FE` in ascending address order.

### Instruction fetch

`cpu_fetch_instruction` reads the 32-bit word at the program counter. It
preserves CPU and RAM state, works while halted, and leaves its output unchanged
on failure. It returns `false` for null required pointers, misaligned PC values,
or addresses where four bytes do not fit inside RAM.

Fetch does not determine whether the word encodes a valid instruction. The
separate decoder currently recognizes ADD. Instruction execution, program-counter
updates, and CPU trap reporting remain later work. See the [fetch flowchart](docs/FLOWCHARTS.md#5-instruction-fetch).

### Instruction fields

`instruction_extract_fields` returns the original 32-bit word and raw slices
for opcode, rd, funct3, rs1, rs2, and funct7. It accepts every 32-bit pattern
and does not determine instruction validity or access CPU state. The format
determines which slices represent registers, constants, or operation selectors.
See the [worked ADD example](docs/HELP.md#worked-example-add).

### Instruction decoding

`instruction_decode` returns an operation kind together with the original word
and its extracted fields. ADD is recognized only when opcode, funct3, and funct7
all match its encoding. Other words return `INSTRUCTION_UNKNOWN`; this includes
valid instructions that this decoder does not support yet, such as SUB and ADDI.
Decoding does not read or modify CPU or RAM state.

## Layout

```text
include/       Public C headers
src/           Core implementations
tests/         Independent test programs
docs/          Learning guide, roadmap, and core flowcharts
Makefile       Build and test targets
```

Headers use `#pragma once`. Source code, comments, documentation, and commit
messages use English.
