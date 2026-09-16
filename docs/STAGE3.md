# Stage 3: From C to a running RV32I program

This stage is implemented. GCC translates C into machine code. The simulated
CPU fetches and executes that machine code; it does not interpret C source.
DWARF debug information lets the monitor associate instruction addresses with
source locations for explanation.

## 1. Install and verify the tools

On Ubuntu, including Ubuntu-26.04 under WSL 2:

```bash
sudo apt update
sudo apt install build-essential python3 libdw-dev libelf-dev gcc-riscv64-unknown-elf binutils-riscv64-unknown-elf
cd /home/axtor/dev/riscv32-studio
make app
riscv64-unknown-elf-gcc -march=rv32i -mabi=ilp32 -print-libgcc-file-name
```

The last command must identify an existing RV32I/ILP32 library. Despite the
`riscv64` executable name, the installed multilib compiler can produce 32-bit
code. The builder explicitly selects `-march=rv32i -mabi=ilp32` and checks
the architecture attributes and flags of the final linked ELF, including its
runtime helpers. It never silently chooses the compiler's default architecture.

The simulator itself is built with the host's GCC. libelf and libdw read symbols
and DWARF metadata on the host; they are not libraries running inside the guest.

## 2. Compile and execute C

From the repository root:

```bash
make run-c
```

This builds `demos/c/learning.c` and opens the monitor. Enter:

```text
help runtime
sources
symbols main
where
step 20
where
source
regs
run
symbols result
symbols calls
quit
```

The first instructions belong to `runtime/start.S`. They initialize the stack,
global pointer, and BSS, then call `main`. Their source location is correctly
shown as assembly. Once execution reaches the C functions, `where` and the trace
refer to C lines.

The learning example sums values greater than two in two arrays. It finishes
with `result=72`, `calls=2`, and `exit=0`. To inspect either variable's bytes,
copy its hexadecimal address from `symbols result` or `symbols calls` into
`mem ADDRESS 4`. Result 72 appears as `48 00 00 00`; calls 2 appears as
`02 00 00 00`. Addresses depend on the compiler and optimization, so obtain
them from the loaded ELF.

For a complete batch run:

```bash
make run-c ARGS="--run"
echo $?
```

Expected shell status: `0`. Add `--trace` to see instructions, source changes,
register writes, memory effects, and the final exit.

To compile another source and choose capacity:

```bash
make guest SOURCE=demos/c/arithmetic.c RAM=1MiB STACK=8KiB GUEST_OUT=build/guest/arithmetic
./build/debug/riscv32-studio --elf build/guest/arithmetic.elf --ram 1MiB --run
echo $?
```

Expected status: `0`. This example checks signed multiplication, division,
remainder, and unsigned 64-bit division implemented by RV32I software helpers.

For multiple C or assembly inputs:

```bash
python3 tools/build_guest.py --source first.c helper.c function.S --output build/guest/program --ram 64KiB
```

Those example filenames must exist; supply your own paths. Assembly modules use
the same RV32I/ILP32 calling convention. The supplied runtime defines `_start`;
your program defines `int main(void)`. Sources are preserved.

## 3. Generated files

| Extension | Contents |
| --- | --- |
| `.elf` | Executable image, entry, segments, symbol table, and DWARF source mapping. |
| `.bin` | Flat little-endian file-backed bytes, starting at load address 0x1000. |
| `.asm` | Annotated GNU objdump listing with addresses, instructions, and available source text. |
| `.map` | Linker map showing sections, symbols, and included library helpers. |
| `.ld` | The concrete linker script used for this RAM and stack configuration. |

The `.asm` output is an inspection listing, including objdump headings and
addresses. It is not a round-trip assembler source. Raw binaries contain no
source text, symbols, entry, RAM capacity, or architecture identifier.

By default, outputs are `build/guest/program.*`. A failed compilation leaves
the previous outputs intact. Successful compilation replaces the selected
output files. `make clean` removes generated files under `build/`.

`OPT=0` is the didactic default. `OPT=2` builds optimized C using the same
pipeline, but the side-by-side comparison interface remains stage 6.
`MODE=release` optimizes the host simulator; `OPT=2` optimizes the guest C.
These are separate settings.

## 4. RAM and runtime layout

RAM starts at address zero. The monitor accepts `--ram 4B` through
`--ram 256MiB`, in multiples of four bytes. The default is `64KiB`.
Units are required and case-sensitive: `B`, `KiB`, or `MiB`.
For example, `64KiB`, `65536B`, and `0x40KiB` all select 65,536 bytes.

The 256 MiB cap is a simulator resource policy. RV32 addresses are still 32 bits.
The monitor keeps current RAM and an independent initial image for reset.
Loading temporarily requires additional storage; available host memory can
therefore prevent an otherwise valid allocation.

The C builder additionally requires RAM and stack sizes aligned to 16 bytes,
and enough room for the chosen code origin, program data, and reserved stack.

Default 64 KiB layout:

| Region | Placement and purpose |
| --- | --- |
| Low RAM | 0x00000000..0x00000FFF; initially zero, available flat RAM. |
| `.text` / read-only data | Start at 0x00001000; includes `_start`, functions, and constants. |
| `.data` | After code, aligned to 16 bytes; initialized globals copied from the executable. |
| `.bss` | After data, aligned to 4 bytes; zero-initialized globals. |
| Free gap | Between the static image and the reserved stack. No allocator is provided. |
| Stack | 0x0000F000..0x0000FFFF; default 4 KiB, grows toward lower addresses. |
| Initial `sp` | 0x00010000, the aligned address immediately above the stack. |

The linker rejects static code/data overlapping the reserved stack.
The stack is a zero-filled ELF load segment, so the loader verifies that the
configured RAM can contain it. Loading with more RAM preserves the ELF's original
stack position; changing the compiled stack position requires rebuilding.

Virtual and physical addresses are identical. There is no flash-to-RAM data
copy: initialized data already resides at its execution address. Startup also
clears BSS so ELF and raw-binary execution have the same initialization behavior.

RAM has no page permissions, protected stack guard, or automatic stack-overflow
detector. The reserved stack size is a layout reservation; a program can corrupt
other in-range RAM if it overflows. The CPU detects accesses outside installed
RAM and misaligned accesses.

## 5. Supported C environment

The target is freestanding C17, RV32I, ILP32: pointers, int, and long have 32 bits.
The startup keeps the stack aligned to 16 bytes and calls `int main(void)`.
Returning from main invokes the teaching exit service: a7=93, a0=return value,
ECALL. The monitor prints the full 32-bit value; batch shell status is its low
eight bits.

Included support:

- Initialized and zero-initialized globals, local variables, arrays, pointers,
  function calls, recursive calls, and structure copies.
- RV32I libgcc integer helpers, including tested multiplication/division/remainder
  and 64-bit division. An operation in C need not have a dedicated CPU instruction.
- `memcpy`, `memmove`, `memset`, and `memcmp` implemented in the guest runtime, declared by `#include "rv32_runtime.h"`.
  GCC's freestanding headers such as `stdint.h` and `stddef.h` are usable.
- C and assembly modules linked by the cross-toolchain.

There is no hosted C library, general `printf`, `malloc/free`, filesystem,
operating system, command-line arguments, TLS, or automatic constructor/destructor
arrays. Unsupported runtime arrays and TLS are rejected. Other ECALL services
stop with a diagnostic. This stage does not promise that arbitrary desktop C
programs can run unchanged or that all libgcc routines have been validated.

## 6. ELF and binary loading

ELF input:

```bash
./build/debug/riscv32-studio --elf build/guest/program.elf --ram 64KiB
```

The loader accepts static ELF32, little-endian RISC-V executables with the
RV32I/ILP32 flags. It validates header/table bounds, segment file and RAM ranges,
alignment, overlap, and a complete aligned executable instruction at the entry.
File-backed bytes are copied; BSS and gaps start at zero.

Unsupported architecture attributes, compressed instructions declared in flags,
floating-point ABI flags, RV32E, dynamic linking, TLS, different physical/virtual
load addresses, and extended ELF section/program counts are rejected.
Base RV32I attribute strings `rv32i`, `rv32i2p0`, and `rv32i2p1` are accepted.
A file without architecture attributes cannot prove which instructions it uses;
an unsupported opcode still traps when executed. The loader does not treat every
word in a code segment as an instruction because segments can contain data.

The ELF file limit is 64 MiB, with at most 1,024 program headers. Symbol and source-range tables are bounded to
262,144 records, source paths to 4,096 bytes, and source files to 4,096 entries.
Malformed or excessive metadata is an input error. A stripped ELF remains
executable with explicit absence of symbols and source mappings.

Raw binary input:

```bash
./build/debug/riscv32-studio --bin build/guest/program.bin --load-address 0x1000 --entry 0x1000 --ram 64KiB --run
```

Both load address and entry are mandatory. They must be aligned, and the entry
must address a complete instruction in the supplied bytes. The binary may also
include data, so its total length need not be divisible by four. Use the RAM
capacity selected when compiling, or enough RAM for the program's actual layout.

ELF determines its own addresses; combining `--elf` with address overrides is
rejected. Exactly one of `--program`, `--elf`, or `--bin` selects the input.

## 7. Understanding source mapping

| Command or display | Meaning |
| --- | --- |
| `where [ADDRESS]` | Show the mapped source location and text; defaults to PC. |
| `sources` | List file indices and paths recorded in the debug information. |
| `source [LINE] [FILE_INDEX]` | Show nearby source lines; defaults to the current location. |
| `symbols [NAME]` | List up to 256 symbols, or find one exact name. |
| `disasm ADDRESS [COUNT]` | Show instructions and their available source locations. |
| `step [N]` / `--trace` | Advance by instructions; print a source label when the mapped file/line changes. |

`source` marks the current mapped line with `>`. A row's address is its first
mapped instruction, not its only instruction. `[no mapped instruction]` means
DWARF supplies no instruction range for that row. It can be a declaration,
comment, blank line, or code transformed/removed during compilation.

An unmapped address is explicitly labeled `<no instruction-to-source mapping>`.
Missing source files are reported while preserving the known path and line.
Source text is read from the recorded filesystem path; rebuild after editing it.
Mappings describe the loaded executable and are not regenerated if a guest
modifies its own instruction bytes.
The monitor does not reconstruct original C from machine code or evaluate local
variables. Those are separate later capabilities.

Both `-O0` and `-O2` can have many instructions per C line, line changes in an
unexpected order, and instructions with no C correspondence. Startup assembly
and library routines also belong to the execution path. The actual PC and
instruction remain authoritative.

## 8. Verification

```bash
make check
make MODE=release test
```

The suite includes 19 native C executables, 19 monitor process tests, and
17 guest/toolchain integration tests. Coverage includes deterministic allocation
failures, transactional replacement, 512 ELF-header mutations, invalid formats,
capacity and entry bounds, initialized/zero globals, reset, raw/ELF agreement,
stack ABI alignment, recursion, C/assembly calls, runtime memory functions,
libgcc arithmetic, stripped metadata, and missing source files.

Every instruction address in the generated learning example, in both `-O0`
and `-O2` builds, is compared with GNU `addr2line` for source mapping.
The optional Unicorn suite separately compares the CPU core's 197 scenarios
and 4,232 instructions. These checks cover the described cases; they are not
formal certification or a guarantee for every C program.

## References

- [GCC RISC-V options](https://gcc.gnu.org/onlinedocs/gcc/RISC-V-Options.html)
- [RISC-V ELF psABI](https://riscv-non-isa.github.io/riscv-elf-psabi-doc/)
- [ELF program headers](https://gabi.xinuos.com/elf/07-pheader.html)
- [GNU addr2line](https://sourceware.org/binutils/docs/binutils/addr2line.html)
