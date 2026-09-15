# Development Roadmap

The machine-state foundation is implemented. The stages below describe planned
work unless an item is explicitly marked complete.

## 1. Machine-state foundation

Completed:

- C17 build with strict compiler warnings and independent tests.
- Separate debug, optimized, and sanitizer configurations.
- CPU state, reset, and checked access to all 32 registers.
- Zero-register behavior through the register access API.
- Separate 64 KiB RAM with reset and checked 8-bit, 16-bit, and 32-bit access.
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

- Single-instruction ADD and SUB execution through `cpu_step`, using the existing fetch,
  decoder, and checked register access functions.
- Low-32-bit addition and subtraction, x0 behavior, and overlapping
  source/destination support.
- Four-byte PC advancement and a 64-bit wrapping instruction counter.
- Distinct step results with state preservation on rejected or halted steps.
- Tests for arithmetic boundaries, register overlap, x0, halt, null arguments,
  PC alignment and bounds, unsupported words, instruction sequences, and execution
  at the end of RAM with both arithmetic operations.

Remaining, in implementation order:

1. Add ADDI and signed I-format immediates, followed by the remaining integer
   arithmetic, logical operations, comparisons, shifts, and upper immediates.
2. Add loads and stores with format-specific offsets, byte order, sign extension,
   alignment rules, and preservation of state on rejected accesses.
3. Add conditional branches and jumps, including correct target computation,
   return-address writes, and fault attribution.
4. Define the teaching execution environment: architectural fault reports,
   ECALL/EBREAK behavior, and FENCE handling appropriate to the machine model.
5. Return step records containing the instruction address, decoded operation,
   register/memory changes, and stop reason. Keep these records independent of UI.
6. Add a minimal command-line runner with step, run, stop/reset, register/memory
   inspection, execution limits, and `--help`.

Use instruction tests, short programs, and selected comparisons with a reference
RISC-V implementation to check the supported architecture behavior. Extend
examples and flowcharts as each instruction family is added.

Exit criterion: reproducible programs can calculate, access RAM, make decisions,
loop, and call/return through the command-line runner. Each stop has a clear cause,
and the full selected RV32I behavior has corresponding tests.

## 3. C compilation and program loading

Implementation order:

1. Make RAM capacity configurable, with explicit size units, allocation and
   address-range checks, and program-fit validation.
2. Define the program memory layout, entry point, stack, initialization of global
   data, and the supported C runtime services and libraries.
3. Integrate a RISC-V compiler, assembler, and linker targeting RV32I and the
   ILP32 calling convention explicitly. Verify toolchain and runtime-library
   compatibility before relying on a compiler configuration.
4. Load ELF programs with their loadable segments, entry point, symbols, and
   debug information. Validate file ranges and all writes into simulated RAM.
5. Support basic raw-binary loading with explicit load address and entry point.
6. Map machine-instruction addresses to C source locations using debug data.
   Represent missing source, unmapped instructions, and lines without executable
   instructions explicitly.

The GCC target settings will be explicit, such as `-march=rv32i -mabi=ilp32`;
host compilation of the simulator core remains a separate build operation.
Project-setting persistence and the complete import/export workflow are finalized
in stage 7.

Exit criterion: compile and run small C programs using variables, decisions,
loops, functions, arrays, and pointers. Show their assembly and available source
mappings through the command-line interface.


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

- Current foundation: CPU/RAM state, checked access, fetch, field extraction,
  and ADD/SUB stepping with tests.
- First complete visual learning path: stages 2 through 4 connect written C to
  compiled instructions and visible machine-state changes.
- Full product scope: the remaining debugger/history, compilation comparison,
  file/project workflows, analysis, examples, help, and distribution milestones
  are complete. Experimental decompilation keeps its explicitly documented status.

The immediate next increment is ADDI and I-format immediates, with a demo that
initializes its own register values before using ADD and SUB.

## References

- [RV32I specification](https://docs.riscv.org/reference/isa/v20260120/unpriv/rv32.html)
- [GCC RISC-V target options](https://gcc.gnu.org/onlinedocs/gcc/RISC-V-Options.html)
