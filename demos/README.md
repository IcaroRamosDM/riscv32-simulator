# Guided RV32I Examples

Run these commands from the repository root after `make app`.
Each .words file contains exact hexadecimal encodings with assembly comments.
The comments are for reading; the loader consumes only the words.
The C examples use the separate cross-compilation workflow described below.
The call.words demo explicitly chooses a 0x10000 stack top; increasing installed
RAM does not relocate that hand-written program's stack.

## 1. Arithmetic without external register setup

```bash
./build/debug/riscv32-studio --program demos/arithmetic.words
```

Enter:

```text
disasm 0 7
step 4
regs
run
quit
```

After four steps, x5=12, x6=7, x7=19, x8=12, and PC=0x10.
The remaining instructions request exit zero. Register changes show before/after
values and a text marker; colors are automatic on terminals.

## 2. Sum, branch, store, and load

```bash
./build/debug/riscv32-studio --program demos/sum.words
```

Enter:

```text
step 3
step 3
regs
run
mem 0x100 4
regs
reset
mem 0x100 4
quit
```

The first three instructions initialize the loop. Each next group of three
instructions adds a value, increments it, and tests the branch. After the
first iteration, x7=1, x5=2, and PC=0x0C.

After `run`, x7 and x9 both contain 15. Memory inspection prints:

```text
0x00000100: 0F 00 00 00
```

The complete program uses 24 attempts: 23 retired instructions and one ECALL.
Reset restores the original zero at 0x100, the initial registers, and entry PC.

## 3. Function call, return address, and stack

```bash
./build/debug/riscv32-studio --program demos/call.words
```

Enter:

```text
step 3
regs
step 3
mem 0xFFF0 16
run
regs
mem 0xFFF8 8
quit
```

The first three steps set sp=0x10000, a0=7, and call the function at 0x1C.
The call saves 0x0C in ra. The next three allocate 16 stack bytes, save ra at
0xFFFC, and save the argument at 0xFFF8.

The function doubles the argument, restores ra/sp, and returns. The caller
keeps the result in x5, then exits with status zero. Final x5=14, sp=0x10000,
and ra=0x0C. The saved stack bytes are:

```text
0x0000FFF8: 07 00 00 00 0C 00 00 00
```

## Batch runs

```bash
./build/debug/riscv32-studio --program demos/arithmetic.words --run --trace
./build/debug/riscv32-studio --program demos/sum.words --run --trace
./build/debug/riscv32-studio --program demos/call.words --run --trace
```

All three exit with shell status zero. Use `echo $?` immediately after a
command to inspect its status. A limit is a pause with status 124, for example:

```bash
./build/debug/riscv32-studio --program demos/sum.words --run --max-steps 5
echo $?
```

This reports five retired instructions and does not complete the sum.

See [Help](../docs/HELP.md) for every command and
[Flowcharts](../docs/FLOWCHARTS.md) for the implemented paths.

## 4. Compile and run C with source mapping

```bash
make run-c SOURCE=demos/c/learning.c
```

Enter:

```text
where
step 20
source
regs
run
symbols result
symbols calls
quit
```

Startup assembly initializes the execution environment, then calls main.
The C example uses globals, locals, arrays, pointers, a conditional, a loop,
and two calls to sum. Expected final result: 72; calls: 2; exit: 0.
Use each symbol's reported address with mem ADDRESS 4 to inspect its bytes.
Reset restores the globals to their original state.

## 5. C arithmetic through software helpers

```bash
make run-c SOURCE=demos/c/arithmetic.c GUEST_OUT=build/guest/arithmetic ARGS="--run"
echo $?
```

Expected status: 0. The program verifies:

| Variable | Expected value |
| --- | --- |
| product | -456765 |
| quotient | -333 |
| remainder | -24 |
| wide_result | 287454020 |

The underlying CPU has no MUL or DIV instruction. Linked RV32I libgcc helpers
implement these C operations using the supported base instructions.
For interactive inspection, omit ARGS="--run" and use symbols NAME, then
mem ADDRESS 4 (or 8 for wide_result).

See [Stage 3](../docs/STAGE3.md) for the generated ELF, binary, listing, source
commands, stack layout, and runtime limits.
