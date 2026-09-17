# How the software works

## 1. Host and guest

The host program is the simulator built with the computer's native C compiler.
The guest program is the C or machine-code program it executes.

`tools/build_guest.py` invokes a RISC-V cross-toolchain. It selects RV32I/ILP32,
links the supplied startup/runtime, checks the resulting executable, and writes
ELF, binary, assembly-listing, map, and linker-script files. It does not translate
C inside the simulated CPU.

~~~mermaid
flowchart LR
    C["Guest C"] --> GCC["RISC-V compiler and linker"]
    GCC --> ELF["ELF executable"]
    ELF --> LOADER["Program loader"]
    LOADER --> RAM["Simulated RAM"]
    RAM --> CPU["C instruction executor"]
    ELF --> DEBUG["Symbols and source mappings"]
    CPU --> MONITOR["Terminal monitor"]
    DEBUG --> MONITOR
~~~

## 2. Modules

| Module | Role |
| --- | --- |
| `memory.c` | Allocate, copy, reset, and access owned RAM with bounds/alignment checks. |
| `instruction.c` | Decode instruction fields and sign-extended immediates. |
| `disassembly.c` | Describe decoded instructions as readable Assembly. |
| `cpu.c` | Fetch and execute one instruction, returning effects or a precise trap. |
| `program.c`, `program_elf.c`, `program_io.c` | Validate input programs before replacing state. |
| `machine.c` | Own CPU/current RAM/reset image and control stepping, limits, and exit. |
| `debug_info.c` | Read ELF symbols and bounded DWARF address-to-source metadata. |
| `source_view.c` | Present source context and symbols without changing execution. |
| `monitor.c`, `main.c` | Parse command-line options and interactive commands. |

The public contracts are in `include/`. CPU execution has no terminal dependency.

## 3. Loading and ownership

The loader builds a temporary program image and validates ranges, segments,
alignment, architecture flags, and the entry point. Only a complete valid image
replaces the current program. A rejected load preserves the old machine.

The machine retains current RAM and an independent initial image. Reset restores
the original program bytes and CPU entry state. Source/debug metadata has its
own owner and lifetime.

Memory-owning structures start zero-initialized and must be destroyed through
their APIs. Plain structure assignment would duplicate owned pointers; use the
provided clone/load operations instead.

## 4. One instruction

`cpu_step_recorded` fetches the four bytes at PC, decodes them, reads operands,
validates the requested effect, and commits successful changes. Sequential
execution uses PC + 4; a taken branch or jump supplies a different next address.

~~~mermaid
flowchart TD
    FETCH["Read instruction at PC"] --> DECODE["Decode operation and operands"]
    DECODE --> CHECK["Validate effect and target"]
    CHECK -->|Valid| APPLY["Update register or RAM and next PC"]
    CHECK -->|Invalid| TRAP["Preserve architectural state and report trap"]
    APPLY --> RECORD["Return a step record"]
    TRAP --> RECORD
~~~

The x0 register always reads zero. A write to x0 can still perform meaningful
work: a load must check and access memory even if its result is discarded.
Arithmetic helpers implement 32-bit wrapping behavior without depending on host
signed-integer overflow.

An optional `CpuStepRecord` contains instruction/operand information, old/new
PC and instruction count, register changes, memory effects, and trap details.
Flags identify which fields are valid.

## 5. Run, stop, and exit

The machine controller repeatedly requests CPU steps until an execution limit,
stop callback/Ctrl-C, halt, trap, or guest exit. A limit keeps state available for
a later run.

The teaching exit convention uses a7 = 93 and a0 = the exit value at ECALL.
The core reports ECALL; the controller interprets this one supported service.
Other services stop with a diagnostic. EBREAK stops at its own address.

Successful instructions increment the retired count. Trapping attempts do not,
so attempted and retired counts can differ.

## 6. C source and machine state

DWARF metadata supplies address ranges and source locations. It annotates the
machine state; it does not control execution. Several instructions can refer
to one C line, and a line can have no mapped instruction.

Source text is read from recorded filesystem paths. It can be missing, and
editing it after compilation does not change the loaded executable. Rebuild
before comparing edited text with execution.

Startup initializes sp, gp, and BSS before calling main. Code begins at 0x1000
by linker policy; RAM starts at zero. Guest pointers are simulated addresses,
never host pointers.

## 7. Verification

Native tests exercise CPU, RAM, loaders, records, and allocation failures.
CLI tests launch the monitor and inspect observable output/status. Guest tests
compile and execute C/Assembly programs and compare debug mappings with
addr2line. The optional Unicorn driver compares selected instruction effects.

See [the C workflow](docs/C_WORKFLOW.md) for runtime/layout details and
[the machine guide](docs/HELP.md) for instruction formats and numeric examples.
