# Development Roadmap

Stages 1 through 3 are complete: the machine-state foundation, RV32I execution,
monitor, configurable RAM, C toolchain, ELF/binary loading, and source mapping
are implemented. Stage 4 is the next development milestone.

## 1. Machine-state foundation

Completed:

- C17 build with strict compiler warnings and independent tests.
- Separate debug, optimized, and sanitizer configurations.
- CPU state, reset, and checked access to all 32 registers.
- Zero-register behavior through the register access API.
- Separate RAM, initially 64 KiB and now configurable, with reset and checked 8-bit, 16-bit, and 32-bit access.
- Little-endian multi-byte access with natural alignment and full-range checks.
- Preservation of output values and RAM on rejected accesses.
- Read-only 32-bit instruction fetch through the program counter, with alignment
  and bounds validation and support for inspection while halted.
- Raw instruction-field extraction with known-word and field-isolation tests.
- ADD and SUB recognition with all operation selectors checked; unknown words retain
  their original encoding and fields.

The implemented behavior is illustrated in [Core Flowcharts](FLOWCHARTS.md).

## 2. RV32I execution

Completed:

- All 40 base RV32I operations with R/I/S/B/U/J immediate decoding.
- Modulo-32-bit arithmetic, signed/unsigned comparisons, logical/arithmetic
  shifts, upper immediates, and x0/source-destination overlap handling.
- Byte/halfword/word loads and stores, sign extension, little-endian byte order,
  strict natural alignment, and unchanged CPU/RAM on rejected instructions.
- Conditional branches, jumps, return-address writes, and precise fault attribution.
- Teaching execution environment with architectural fault records, ECALL exit
  service (a7=93), EBREAK stop, and conservative sequential FENCE behavior.
- Optional UI-independent step records for instructions, PC/count, register
  and memory effects, and traps.
- A command-line monitor with step/run/stop/reset, inspection, disassembly,
  bounded execution, Ctrl-C, change highlighting, and offline contextual help.
- Transactional loading of annotated instruction-word text files. This is a
  stage-2 teaching input, now complemented by the stage-3 ELF/raw-binary loaders.
- Three self-initializing demos: arithmetic, sum loop with RAM, and a function
  call/return with a stack frame.
- Fourteen native C suites and thirteen CLI process checks passing in debug,
  optimized, and address/undefined-behavior sanitizer configurations.
- Independent comparison with Unicorn 2.1.4: 197 scenarios and 4,232 matching
  instructions, covering every retiring base operation, deterministic random
  sequences, and all three demos. Trap and strict-alignment semantics have
  native tests. This is selected validation, not formal certification.
- Updated learning guide and flowcharts for the full implemented path.

Exit criterion met: reproducible programs calculate, access RAM, make decisions,
loop, and call/return through the monitor. Stops report their cause, and the
selected RV32I behavior has corresponding tests.

## 3. C compilation and program loading

Completed:

1. Owned configurable RAM: 4B through 256MiB, explicit units, checked allocation,
   deep copies, reset without allocation, and transactional replacement.
2. Defined C runtime/layout: code at 0x1000, initialized data, zero-filled BSS,
   configurable 16-byte-aligned stack, main entry, and ECALL exit service.
3. RV32I/ILP32 compiler/assembler/linker driver with final-ELF compatibility
   checks, matching libgcc, and guest memory routines.
4. Validated ELF32 segment loading, entry, architecture attributes, symbols,
   and bounded DWARF metadata.
5. Raw-binary loading requiring explicit load and entry addresses.
6. Source range lookup, annotated tracing/disassembly, source context, symbol
   inspection, and explicit missing-source/unmapped-line indicators.

The CLI retains instruction-first stepping. Multiple instructions from one
source line keep the same label during tracing. Both unoptimized and optimized
DWARF mappings are checked against GNU addr2line; neither implies one-to-one C
translation.

Validation includes 19 native C suites, 19 CLI tests, and 17 guest integration
tests in debug, optimized-host, and ASan/UBSan configurations, plus the existing
Unicorn comparison. Tests cover allocation failure, malformed ELF, binary/ELF
agreement, C control flow, arrays/pointers, recursion, C/Assembly calls, stack
alignment, initialized/BSS globals, runtime memory routines, and integer
libgcc helpers.

Exit criterion met: compile and execute the representative C programs through
the CLI and inspect their assembly and available source mappings. See the
[stage-3 guide](STAGE3.md) for commands, layout, supported libraries, limits,
and expected results.

The graphical workspace follows in stage 4. Source-level stepping, local-variable
evaluation, and history remain stage 5. Side-by-side optimization comparison is
stage 6; complete import/export/project persistence is stage 7.


## 4. Desktop interface

- Build a workspace inspired by VM8 Studio, with a C editor and synchronized
  assembly, register, memory, and execution-status views.
- Make one machine instruction the default execution step. Keep the C highlight
  on its source line while multiple mapped assembly instructions execute.
- Highlight changes using color together with before/after values. Provide
  hexadecimal, decimal, and binary views.
- Integrate compile/load controls, diagnostics linked to source locations, and
  clear displays for unavailable or optimized-out source information.
- Keep run/pause controls responsive while programs execute.
- Provide searchable offline Help/F1, a glossary, contextual explanations, and
  examples for instructions and formats R, I, S, B, U, and J. Use progressive
  detail and accessible colors/text rather than relying on color alone.

Exit criterion: the user can write a small C program, compile it, and follow its
execution visually from assembly instructions to register and RAM changes.
The core has no dependency on the graphical interface.


## 5. Debugging and execution history

- Add address and source breakpoints, run/pause/reset, C source stepping, and
  function step-into, step-over, and step-out behavior.
- Inspect variables, stack frames, and pointers when debug information permits;
  identify unavailable values explicitly.
- Use step records to explain why execution stopped and what changed.
- Record enough previous state for reverse stepping and replay. Define storage
  limits, history truncation, and the handling of input/output effects.
- Keep debugger controls consistent between the command-line runner and UI.

Exit criterion: pause at a selected location, inspect a function's state, step
forward, and restore earlier recorded CPU/RAM states reliably within the
documented history limits.


## 6. Compilation comparison

- Build the same C source in a didactic configuration such as `-O0 -g` and an
  optimized configuration such as `-O2 -g`.
- Keep separate programs, machine states, assembly listings, and source mappings
  so each version can advance independently.
- Explain missing, reordered, merged, or expanded source mappings and unavailable
  variables. Unoptimized compilation also has no one-to-one mapping guarantee.
- Compare instruction sequences and execution effects using defined example
  programs with the same input.

Exit criterion: open both compilations together, execute them independently, and
understand a concrete optimization through the generated code and visible state.


## 7. Import, export, and project files

- Complete the UI and command-line workflows for assembly sources, ELF files,
  and raw binaries using the loader/toolchain foundations from stage 3.
- Export assembly listings, executable files, and raw binaries. Document which
  metadata each format preserves.
- Assemble imported sources and disassemble imported machine code.
- Require architecture, load address, and entry point when a raw binary does not
  supply that information.
- Save complete projects with original C sources, RAM capacity, build settings,
  debug data, and workspace state.
- Report malformed, incompatible, or oversized files clearly.

Exit criterion: save and reopen a complete project and round-trip representative
programs through the supported import/export paths with the required metadata.


## 8. Program analysis

- Add cross-references between instructions, labels, functions, and data.
- Show control-flow graphs and call graphs with navigation to the relevant code.
- Evaluate and integrate approximate decompilation as an optional experimental
  capability for imported programs.
- Label reconstructed C as an approximation. Machine code does not generally
  preserve original names, comments, types, structure, or source text.

Exit criterion: inspect the paths and relationships in an imported program.
Document the outcome and limits of the experimental decompilation work; when
enabled, reconstructed C is clearly distinct from preserved original sources.


## 9. Examples and distribution

- Maintain guided examples for arithmetic, decisions, loops, functions, arrays,
  pointers, stack behavior, and optimization.
- Exercise the complete C-to-execution path, file workflows, debugger, reverse
  stepping, offline help, and error messages.
- Check responsiveness and bound memory/history use with larger examples.
- Package the desktop application for Linux and Windows, with a reproducible
  toolchain setup and clear runtime requirements.
- Test installation and example execution in clean environments.

Exit criterion: both platform packages support the documented learning workflows
and pass release acceptance tests.

## Completion milestones

- Current foundation: stages 1 through 3 connect C compilation to RV32I execution
  and the tested monitor, with configurable RAM, loaders, source mapping, demos,
  step records, diagnostics, and help.
- First complete visual learning path: stages 2 through 4 connect written C to
  compiled instructions and visible machine-state changes.
- Full product scope: the remaining debugger/history, compilation comparison,
  file/project workflows, analysis, examples, help, and distribution milestones
  are complete. Experimental decompilation keeps its explicitly documented status.

The next stage is the desktop interface: synchronized C, Assembly, registers,
and memory views using the implemented loaders, metadata, and step records.

## References

- [RV32I specification](https://docs.riscv.org/reference/isa/v20260120/unpriv/rv32.html)
- [GCC RISC-V target options](https://gcc.gnu.org/onlinedocs/gcc/RISC-V-Options.html)
