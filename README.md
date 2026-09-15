# RISC-V Studio

An educational simulator connecting machine instructions, registers, and RAM
through instruction-by-instruction execution. The simulator is written in C17.
Stage 2 is complete: the RV32I core and command-line monitor are available.

The planned desktop application will connect C source to this execution model.
C compilation, configurable RAM, ELF loading, and C source mappings are stage 3;
the graphical interface follows in stage 4. See the [roadmap](docs/ROADMAP.md).

## Quick start

Requirements: Linux, GCC with C17 support, GNU Make, Python 3.9 or newer, and
standard command-line tools including `find`. Development and validation use
Ubuntu under WSL 2. The simulator is a native host program; no RISC-V cross
compiler is required for the current demos.

From the repository root:

```bash
make
./build/debug/riscv32-studio --help
./build/debug/riscv32-studio --program demos/sum.words --run --trace
```

The sum demo calculates 1 + 2 + 3 + 4 + 5, stores 15 at RAM address 0x100,
loads it into x9, and exits with status zero. The final line includes:

```text
Stopped: program exited; attempts=24; retired=23; PC=0x0000002C; exit=0
```

For interactive use:

```bash
./build/debug/riscv32-studio --program demos/sum.words
```

At the `rv32>` prompt:

```text
help
help formats
help addi
step
regs
run
mem 0x100 4
reset
quit
```

`step` shows before/after values. Changed registers are marked with text and
highlighted in cyan when output is a terminal. `--color` and `--no-color`
override automatic color selection. Ctrl-C interrupts a run between instructions.

Read the [learning guide](docs/HELP.md), [flowcharts](docs/FLOWCHARTS.md), and
[demo walkthroughs](demos/README.md).

## Implemented architecture

- All 40 RV32I base operations: integer ALU, shifts, signed/unsigned comparisons,
  upper immediates, loads/stores, branches/jumps, FENCE, ECALL, and EBREAK.
- 32 registers, hardwired x0 through the access API, 32-bit PC, and a wrapping
  64-bit count of retired instructions.
- Separate 64 KiB RAM, little-endian accesses, and natural alignment.
- Precise faults: a rejected instruction preserves CPU/RAM state. Trap records
  identify the faulting instruction, cause, and relevant address or word.
- Optional step records with instruction, PC/count changes, register effects,
  memory effects, branch outcome, and trap details; no terminal dependency.
- A teaching execution environment: ECALL with a7=93 exits using a0.
  Other services stop with a diagnostic. EBREAK stops at the breakpoint.
- Transactional text-word loading, disassembly, bounded execution, reset to the
  initial image, register/memory inspection, and offline command help.

This is an instruction-level, single-hart machine, not a clock-cycle or
privileged-system simulator. M, C, CSR, and FENCE.I extensions are outside the
current target. There is no operating system or general Linux syscall support.
FENCE is satisfied by the already sequential memory model.

## Build and verification

| Command | Action |
| --- | --- |
| `make` | Build the monitor and C test executables in debug mode. |
| `make app` | Build only the monitor. |
| `make test` | Run 14 C suites and 13 CLI process tests. |
| `make check` | Run the C and CLI tests in debug and sanitizer modes. |
| `make MODE=release test` | Run the same tests with optimization. |
| `make cli-test` | Run only the CLI process tests. |
| `make run ARGS="..."` | Build and launch the monitor with arguments. |
| `make reference-test PYTHON=.../bin/python` | Compare with Unicorn in an optional virtual environment. |
| `make clean` | Remove the generated build directory. |
| `make help` | Show build targets and options. |

Debug uses `-O0 -g3`, release uses `-O2 -g`, and sanitize uses AddressSanitizer
and UndefinedBehaviorSanitizer. All use C17 and warnings as errors. Test
assertions stay enabled in every mode; setup and execution are outside assertions.

Build outputs are isolated by mode. Header dependencies, compiler settings, and
source-list changes trigger rebuilding. New `src/**/*.c` and
`tests/**/test_*.c` files are discovered automatically; each C test has its own
main. Shared helpers live under `tests/support/`.
Override `CC`, `CPPFLAGS`, `CFLAGS`, `LDFLAGS`, `LDLIBS`, or `PYTHON` as needed.

### Optional independent comparison

The ordinary build and tests require no downloaded Python packages. For the
reference comparison, install the pinned dependency in a virtual environment:

```bash
python3 -m venv build/reference-venv
build/reference-venv/bin/python -m pip install -r tests/reference/requirements.txt
make reference-test PYTHON=build/reference-venv/bin/python
```

The Ubuntu package `python3-venv` must be installed to create this environment.
`make clean` also removes an environment created under `build/`.

With Unicorn 2.1.4 the comparison reports 197 scenarios and 4,232 matching
instructions. It covers all 38 retiring RV32I operations, deterministic random
sequences, and all three demos. PC and all registers are checked after every
step, a 1 KiB data window is checked after every step, and the entire RAM is
compared at the end of every scenario. Native suites test precise traps,
ECALL/EBREAK, strict misalignment, rejected inputs, and unchanged state on faults.
These are selected architectural checks, not formal RISC-V certification.

## Core interfaces

| Header | Responsibility |
| --- | --- |
| `cpu.h` | CPU state, checked registers, fetch, step, and step records. |
| `memory.h` | Fixed RAM and checked little-endian 8/16/32-bit access. |
| `instruction.h` | Raw fields, RV32I decoding, disassembly, and instruction descriptions. |
| `program.h` | Read an annotated word image without partial output on failure. |
| `machine.h` | Program lifecycle, reset, execution limit, stop callback, and exit service. |
| `monitor.h` | Command-line application entry. |

Reset/init functions require valid object pointers. Checked APIs handle null
arguments, but cannot validate dangling pointers. Output objects and step
records must be separate writable storage. The exposed structures support
inspection; direct writes bypass API invariants.

`cpu_step_recorded` attempts one instruction. On success, the instruction's
specified effects are committed, PC advances or changes to a target, and the
counter increments modulo 2^64. Non-OK results preserve CPU/RAM and do not
automatically set `halted`. `cpu_step` is the same operation without a record.

The machine controller handles the ECALL exit convention outside the CPU.
`machine_step` and `machine_run` resume a manually stopped CPU. An exited
program requires reset. Run callbacks execute synchronously between completed
steps and must not mutate machine state; cancellation uses the stop callback.

See the [step record contract](docs/HELP.md#step-record-contract) and
[environment rules](docs/HELP.md#execution-environment).

## Layout

```text
include/          Public headers
src/              CPU, RAM, decoder, loader, controller, and monitor
tests/            Independent C suites
tests/cli/        Monitor process tests
tests/reference/  Optional Unicorn comparison
demos/            Annotated instruction words and guided examples
docs/             Help, roadmap, and flowcharts
```

Headers use `#pragma once`. Code, comments, documentation, and commit messages
use English.
