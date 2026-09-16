# RISC-V Studio

An educational simulator connecting C source, RV32I instructions, registers,
and RAM through instruction-by-instruction execution. The simulator is written
in C17. Stages 1 through 3 are complete: CPU execution, command-line monitoring,
configurable memory, C compilation, ELF/binary loading, and source mapping.

The desktop interface follows in stage 4. See the [roadmap](docs/ROADMAP.md).

## Quick start

Development and validation use Ubuntu-26.04 under WSL 2. Install the host and
cross-compilation dependencies:

```bash
sudo apt update
sudo apt install build-essential python3 libdw-dev libelf-dev gcc-riscv64-unknown-elf binutils-riscv64-unknown-elf
cd /home/axtor/dev/riscv32-studio
make run-c
```

This compiles `demos/c/learning.c` for RV32I/ILP32 and opens the monitor. Enter:

```text
help
help source
where
step 20
source
regs
run
symbols result
quit
```

The first steps execute the startup assembly before calling C's main function.
The example finishes with result 72, two calls to the sum function, and exit
status zero. A source line can correspond to several instructions: stepping
always follows the actual assembly instruction.

Changed registers show before/after values and a text marker, plus cyan on a
terminal. `--color` and `--no-color` override automatic colors. Ctrl-C stops a
run between instructions.

For a batch run:

```bash
make run-c ARGS="--run --trace"
echo $?
```

Expected shell status: 0. The original annotated-word examples also work:

```bash
./build/debug/riscv32-studio --program demos/sum.words --run
```

Read the [stage-3 guide](docs/STAGE3.md) for the complete C workflow, memory
layout, toolchain settings, generated artifacts, and runtime limits.
The [learning guide](docs/HELP.md), [flowcharts](docs/FLOWCHARTS.md), and
[demo walkthroughs](demos/README.md) explain the execution model.

## Implemented architecture

- All 40 RV32I base operations, 32 registers, x0 behavior, a 32-bit PC, and a
  wrapping 64-bit retired-instruction count.
- Configurable little-endian RAM: default 64 KiB, from 4 bytes to 256 MiB in
  multiples of four, with checked natural alignment and full-range accesses.
- Precise faults and optional step records, without terminal dependencies in
  CPU execution.
- C17 freestanding RV32I/ILP32 compilation, explicit memory layout, startup,
  integer libgcc helpers, and basic memory routines.
- Transactional annotated-word, ELF32, and raw-binary loaders, with independent
  current/initial memory images for reset.
- ELF symbol inspection, DWARF instruction-to-source mapping, annotated
  disassembly, and explicit missing-source/unmapped-line states.
- Step/run/stop/reset, bounded execution, register/RAM inspection, and offline
  contextual help.
- Teaching environment: a7=93 + ECALL exits with a0. Other services trap;
  EBREAK stops at its instruction.

This is a single-hart instruction-level machine. M, C, CSR, and FENCE.I
extensions, privileged execution, an operating system, general Linux syscalls,
and hosted libc are outside the current target. Multiplication/division in C
use RV32I software helpers. FENCE is satisfied by the sequential memory model.
The flat RAM model has no page permissions or automatic stack guard.

## Build and verification

| Command | Action |
| --- | --- |
| `make` | Build the monitor and native C test executables. |
| `make app` | Build only the monitor. |
| `make guest` | Compile the default C example to ELF, binary, listing, map, and linker script. |
| `make run-c` | Compile C and open the monitor. |
| `make run-c ARGS="--run"` | Compile C and run it in batch. |
| `make test` | Run 19 native C suites, 19 CLI tests, and 17 guest integration tests. |
| `make check` | Run all suites in debug and sanitizer modes. |
| `make MODE=release test` | Run all suites with the host simulator optimized. |
| `make cli-test` / `make guest-test` | Run one process-test group. |
| `make run ARGS="..."` | Build and launch the monitor with arguments. |
| `make reference-test PYTHON=.../bin/python` | Compare the core with Unicorn. |
| `make clean` | Remove generated files under build/. |
| `make help` | Show targets and options. |

Guest build settings: `SOURCE=demos/c/learning.c`, `RAM=64KiB`, `STACK=4KiB`,
`OPT=0`, `GUEST_OUT=build/guest/program`, and
`CROSS_COMPILE=riscv64-unknown-elf-`. For multiple sources, invoke
`python3 tools/build_guest.py --source file.c helper.c function.S`.

Host debug uses `-O0 -g3`, release uses `-O2 -g`, and sanitize uses
AddressSanitizer and UndefinedBehaviorSanitizer. All configurations use C17 and
warnings as errors. Test setup and execution remain outside assertions.
The host's `MODE` and guest's `OPT` are independent.

Host outputs are isolated by mode. Header dependencies, compiler settings, and
source-list changes trigger rebuilding. Native source/test files are discovered
automatically. Shared test helpers live under tests/support/. All integration
tests use Python's standard library and the installed cross-toolchain.

The guest tests compare source mappings with GNU addr2line, execute C/assembly
calls and runtime helpers, and cover stripped/missing metadata, incompatible
inputs, initialized/BSS data, reset, and raw/ELF agreement.

### Optional independent CPU comparison

```bash
sudo apt install python3-venv
python3 -m venv build/reference-venv
build/reference-venv/bin/python -m pip install -r tests/reference/requirements.txt
make reference-test PYTHON=build/reference-venv/bin/python
```

Unicorn 2.1.4 comparison: 197 scenarios and 4,232 matching instructions,
covering the 38 retiring RV32I operations, deterministic random sequences,
and the three annotated-word demos. PC/registers and a 1 KiB data window are
compared after each instruction; all RAM is compared at each scenario's end.
Native tests cover traps, misalignment, transactional loads, and allocation
failure. These are selected checks, not formal certification.
`make clean` also removes a virtual environment placed under build/.

## Core interfaces and ownership

| Header | Responsibility |
| --- | --- |
| cpu.h | CPU state, register access, fetch, step, and step records. |
| memory.h | Owned dynamic RAM, reset/copy/clone, checked 8/16/32-bit access. |
| instruction.h | Raw fields, RV32I decode/disassembly, and instruction descriptions. |
| program.h | Transactional word, ELF, and raw-binary loading. |
| machine.h | Initial/current images, reset, execution limits, stop, and exit. |
| debug_info.h | Independently owned symbols, source paths, and instruction ranges. |
| source_view.h | Text presentation of source locations and symbols. |
| monitor.h | Command-line application entry. |

Memory, Program, Machine, and DebugInfo owners start zero-initialized. Destroy
functions release storage and clear owners. Do not duplicate owned pointers by
assigning these structures; use the provided clone/load APIs. machine_init is
for fresh storage. Loading is transactional; reset copies from the initial
image without allocating. Metadata belongs to DebugInfo, separately from the
Machine's reset image.

Checked APIs reject invalid arguments but cannot validate dangling pointers.
Output/record storage must be separate. Direct writes to exposed structures
bypass API invariants. Execution callbacks are synchronous and must not mutate
the machine. The stop callback provides cancellation.

See the [step record contract](docs/HELP.md#step-record-contract) and
[environment rules](docs/HELP.md#execution-environment).

## Layout

```text
include/          Public C headers
src/              CPU, RAM, decoders, loaders, metadata, controller, and monitor
runtime/          Guest startup, memory routines, and linker template
tools/            Cross-compilation driver
tests/            Native C suites
tests/cli/        Monitor process tests
tests/guest/      Cross-toolchain/runtime/source-mapping integration tests
tests/reference/  Optional Unicorn comparison
demos/            Annotated words and C examples
docs/             Help, stage-3 guide, roadmap, and flowcharts
```

Headers use `#pragma once`. Code, comments, documentation, and commit messages
use English.
