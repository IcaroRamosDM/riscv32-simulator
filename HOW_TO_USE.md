# How to use RISC-V Simulator

## 1. Install and build

The validated environment is Ubuntu 26.04 x86-64, including WSL 2.
Run these commands in the Linux terminal:

~~~bash
sudo apt update
sudo apt install git build-essential python3 libdw-dev libelf-dev gcc-riscv64-unknown-elf binutils-riscv64-unknown-elf
git clone https://github.com/IcaroRamosDM/riscv32-simulator.git
cd riscv32-simulator
make app
~~~

The result is `build/debug/riscv32-studio`. The `riscv64` tool prefix also supports
32-bit RV32I/ILP32; the builder selects and verifies that target explicitly.

## 2. First complete C session

~~~bash
make run-c
~~~

At the `rv32>` prompt, enter:

~~~text
help
where
step 20
source
regs
run
symbols result
symbols calls
quit
~~~

Execution begins in startup Assembly and eventually reaches C. The learning
example completes with result 72, calls 2, and exit status 0. Use the addresses
printed by `symbols` with `mem ADDRESS 4`: result 72 appears as `48 00 00 00`,
and calls 2 as `02 00 00 00`. Addresses depend on the compiler.

## 3. Follow individual instructions

Reopen the example with `make run-c`. `step` advances one instruction; `step 10`
advances up to ten. `where` describes the current source mapping.
`source` displays surrounding source text. Several steps may keep the same C
line marked because the compiler generated several instructions.

`regs` shows the same register bits in hexadecimal, signed, and unsigned form.
Changed registers carry a text marker and terminal color. `mem ADDRESS COUNT`
inspects bytes; addresses are simulated RAM addresses.

Use `help source`, `help formats`, `help runtime`, or `help addi` for contextual
explanations. The machine guide explains registers and memory in more detail.

## 4. Run without the interactive prompt

~~~bash
make run-c ARGS="--run"
echo $?
~~~

Expected shell status: 0. For instruction-by-instruction output:

~~~bash
make run-c ARGS="--run --trace"
~~~

Ctrl-C requests a stop between instructions. In an interactive session, a run
limit pauses execution while retaining state. `reset` restores the initial CPU
and RAM; `quit` closes the monitor.

## 5. Use the supplied Assembly-word examples

~~~bash
make app
./build/debug/riscv32-studio --program demos/sum.words
~~~

Enter `step`, `regs`, `run`, `mem 0x100 4`, then `quit`. The sum example stores 15
as `0F 00 00 00`. The [demo guide](demos/README.md) explains every word and the
expected registers. A `.words` file contains encoded instruction words, not
assembler mnemonics.

## 6. Choose source, RAM, stack, and optimization

~~~bash
make guest SOURCE=demos/c/arithmetic.c RAM=1MiB STACK=8KiB OPT=2 GUEST_OUT=build/guest/arithmetic
./build/debug/riscv32-studio --elf build/guest/arithmetic.elf --ram 1MiB --run
echo $?
~~~

Expected shell status: 0. Replace `SOURCE` with your C file. It must define
`int main(void)` and use the documented freestanding environment.

`OPT` controls guest optimization. `MODE=release` separately optimizes the host
simulator. Use different `GUEST_OUT` prefixes to retain several builds.
C compilation requires RAM/stack capacities aligned to 16 bytes and sufficient
space for code, data, and the reserved stack.

## 7. Files and source inspection

The builder writes `.elf`, `.bin`, `.asm`, `.map`, and `.ld` files. Prefer ELF
when inspecting symbols and source mappings. Raw binaries require explicit load
and entry addresses and do not preserve source metadata.

The `.asm` output is an annotated disassembly listing intended for inspection.
The [complete C workflow](docs/C_WORKFLOW.md) gives exact raw-binary commands,
multi-source compilation, generated-file details, and memory layout.

## 8. Common problems

| Observation | Explanation or next step |
| --- | --- |
| Cross-compiler missing | Install the RISC-V GCC/binutils packages listed above. |
| Build reports unsupported symbols | Hosted printf, malloc, and OS services are unavailable. |
| No C source at the first steps | Startup Assembly runs before main; use `where` and continue. |
| A source file is missing | Restore the original source path recorded by ELF or rebuild. |
| One C line appears for several instructions | This is normal compiler/source mapping behavior. |
| Trap on memory access | Inspect address, width, RAM capacity, and natural alignment. |
| Run reaches a limit | Inspect state and continue, or fix an unintended loop. |
| Repeated EBREAK stop | EBREAK stays at its instruction; reset to restart. |
| Changing RAM does not move the stack | Rebuild; the ELF records its compiled layout. |

## 9. Verify the build

~~~bash
make check
make MODE=release test
~~~

Checks cover 19 native C suites, 19 CLI tests, and 17 guest integration tests.
The README also explains the optional independent Unicorn comparison.

## Further reading

- [Software design](HOW_IT_WORKS.md)
- [Registers, instructions, and execution rules](docs/HELP.md)
- [Flowcharts](docs/FLOWCHARTS.md)
- [Scope and limits](docs/SCOPE.md)
