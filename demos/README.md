# Guided RV32I Examples

Run these commands from the repository root after `make app`.
Each .words file contains exact hexadecimal encodings with assembly comments.
The comments are for reading; the loader consumes only the words.

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
