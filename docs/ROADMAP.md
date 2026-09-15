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

Next:

- Decode instruction fields and identify supported operations.
- Add execution, program-counter updates, and CPU traps in the next stage.

The implemented behavior is illustrated in [Core Flowcharts](FLOWCHARTS.md).

## 2. RV32I execution

- Decode and execute the base integer instruction set.
- Define reset, stop, trap, and instruction-counting behavior.
- Report each step's instruction address and changes to machine state.
- Test arithmetic boundaries, branches, jumps, loads, stores, and invalid
  instructions against the selected architecture behavior.
- Add a minimal command-line runner and reproducible example programs.

## 3. C compilation and program loading

- Integrate a RISC-V C toolchain and define the supported runtime environment.
- Load ELF programs with entry points, sections, symbols, and debug information.
- Map instruction addresses to C source locations when information is available.
- Explain unmapped instructions and source lines without executable code.

## 4. Desktop interface

- Provide synchronized C, assembly, register, and memory views in a workspace
  inspired by VM8 Studio.
- Make one machine instruction the primary execution step.
- Keep the C highlight on its mapped source line while related assembly
  instructions execute.
- Highlight register and memory changes with color and before/after values.
- Support hexadecimal, decimal, and binary views, contextual explanations,
  progressive detail, and accessible presentation.

## 5. Debugging and execution history

- Add run, pause, reset, breakpoints, C source stepping, and function stepping.
- Inspect variables, stack frames, and pointers when debug information permits.
- Show why execution stopped and what changed during each step.
- Add recorded execution history and reverse stepping, with defined limits on
  storage and reversible effects.

## 6. Compilation comparison

- Compare a didactic configuration such as `-O0 -g` with an optimized
  configuration such as `-O2 -g`.
- Show the generated instructions and source mappings for each build.
- Allow each assembly listing to advance independently.
- Make missing, reordered, merged, or expanded source mappings visible.
- Avoid promising a one-to-one correspondence between C and assembly, including
  in unoptimized builds.

## 7. Import, export, and project files

- Export assembly listings, executable files, and raw binaries.
- Open assembly sources and assemble them for the supported target.
- Open ELF files and raw binaries and disassemble their machine code.
- Require the necessary architecture, load-address, and entry-point information
  when importing a raw binary.
- Preserve original C sources, build settings, debug data, and workspace state
  in complete project files.

## 8. Program analysis

- Add cross-references, control-flow views, and call graphs.
- Explore optional decompilation for imported programs.
- Clearly identify reconstructed C as an approximation: machine code does not
  generally preserve the original source, names, comments, or structure.

## 9. Examples and distribution

- Provide guided examples for arithmetic, decisions, loops, functions, arrays,
  and pointers.
- Validate the complete C-to-execution workflow and error messages.
- Package the desktop application for Linux and Windows.
