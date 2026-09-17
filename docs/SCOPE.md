# Scope and limits

RISC-V Simulator is a complete educational command-line project for observing
how freestanding C becomes RV32I instructions and changes machine state.

## Delivered capabilities

- C17 instruction-level CPU, with all 40 RV32I base operations, 32 integer
  registers, a 32-bit program counter, and little-endian byte-addressed RAM.
- Checked instruction/data alignment, bounds, traps, and explicit step records.
- Configurable RAM from 4 bytes to 256 MiB, with an independent reset image.
- Transactional annotated-word, ELF32, and raw-binary program loading.
- RV32I/ILP32 C compilation, startup code, memory routines, and integer helpers.
- ELF symbols, bounded DWARF source mappings, source display, and disassembly.
- Interactive or batch execution, bounded run lengths, Ctrl-C, reset,
  register/memory inspection, tracing, and contextual help.
- Native, command-line, guest-program, and optional independent-reference tests.

## Deliberate boundaries

This model has one execution context and models instruction effects. It does
not model electrical signals, pipeline timing, caches, privileged modes, virtual
memory, interrupts, an operating system, or general Linux system calls.

The guest instruction set is RV32I. Optional M, A, F, D, C, CSR, and FENCE.I
extensions are outside the target. C multiplication and division can execute
through matching RV32I software helpers.

C programs use a freestanding runtime. A hosted standard library, filesystem,
heap allocator, interactive standard input/output, and arbitrary desktop C
program compatibility are outside the execution environment.

The monitor exposes the commands listed in its help. Source-level breakpoints,
local-variable evaluation, reverse stepping, graphical views, project sessions,
and decompilation are outside this edition. Optimized debug mappings do not
guarantee a one-to-one relationship between C lines and instructions.

## Validation

Run `make check` for debug and address/undefined-behavior sanitizer checks.
Run `make MODE=release test` for optimized-host checks. These cover 19 native C
suites, 19 CLI process tests, and 17 guest integration tests.

`make reference-test` runs the optional Unicorn comparison documented in the
README. Tests sample defined behavior and malformed inputs; they do not constitute
formal verification or certification.
