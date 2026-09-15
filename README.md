# RISC-V Studio

An educational desktop simulator being developed to connect C source code,
RISC-V assembly, registers, and memory through instruction-by-instruction execution.

The project targets RV32I and uses a C17 simulator core. It is currently at the
machine-state foundation stage: instruction execution, C compilation, file
loading, and the graphical interface are planned work.

## Implemented

- CPU state with 32 registers, a program counter, an instruction counter, and a
  halted flag.
- CPU reset and checked register access. Reads of `x0` return zero; writes to
  `x0` succeed and discard the value.
- A separate 64 KiB RAM object with reset and checked byte reads and writes.
- Independent tests for CPU reset, register access, and RAM access.
- Debug, optimized, and sanitizer build configurations.

The development roadmap is in [docs/ROADMAP.md](docs/ROADMAP.md).

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
CPU register tests passed.
Memory tests passed.
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
This capacity is an initial simulator configuration; the address parameter
remains 32 bits wide.

`memory_reset` clears every byte. `memory_read_u8` and `memory_write_u8` return
`false` for null required pointers or addresses outside this range. Validation
happens before indexing the array, and rejected accesses leave RAM and any
provided output value unchanged.

Both reset functions require a valid pointer to their respective object.
Access functions accept valid object pointers or null; they cannot validate
arbitrary dangling pointers. Read outputs should point to separate writable
storage.

Multi-byte memory access, byte-order handling, and instruction fetch are the
next implementation steps.

## Layout

```text
include/       Public C headers
src/           Core implementations
tests/         Independent test programs
docs/          Development roadmap
Makefile       Build and test targets
```

Headers use `#pragma once`. Source code, comments, documentation, and commit
messages use English.
