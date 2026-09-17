# Learning Guide

## Start here

From the repository root:

```bash
make app
./build/debug/riscv32-studio --help
./build/debug/riscv32-studio --program demos/sum.words
```

At `rv32>`, type `help`, `help formats`, or `help addi`. Commands are
case-sensitive; mnemonic help accepts either case. Use `quit` to leave.
For C compilation, ELF/binary input, source inspection, and the memory/runtime
layout, follow the [C workflow](C_WORKFLOW.md). Use `make run-c` to start its C demo.
The [demo walkthroughs](../demos/README.md) supply expected results.
The [flowcharts](FLOWCHARTS.md) show the current CPU and monitor paths.

## Essential terms

| Term | Meaning |
| --- | --- |
| Register | A 32-bit storage location inside the CPU, numbered x0 through x31. |
| Register number | Which location to select; x2 selects location 2. |
| Register value | The bits stored there; x2 might contain 65,536. |
| PC | Program counter: the address of the next instruction to attempt. |
| Instruction | A 32-bit word encoding an operation and its operands. |
| Opcode / funct fields | Bit fields that select an operation. |
| Immediate | A constant stored in the instruction itself. |
| Format | The arrangement of fields in the instruction word. |
| Byte address | A location in RAM; consecutive addresses select consecutive bytes. |
| Retire | Complete an instruction successfully and commit its effects. |
| Trap | A reported condition that prevents normal completion, including faults and ECALL. |
| Hart | One independently executing RISC-V hardware thread; this simulator has one. |

The simulator executes encoded RV32I instructions loaded from words, ELF, or
raw binary. The C toolchain produces those instructions and source metadata.
One monitor step means one machine instruction, even when several instructions
map to the same C line.

## Worked example: ADD

Start with x1=99, x2=5, and x3=7:

```asm
add x1, x2, x3
```

Read this as: add the value in x2 to the value in x3, then put the result in x1.
The destination comes first. x1 becomes 12; x2 and x3 keep their values.
The register numbers 1, 2, and 3 are not the values being added.

| Field | Instruction bits | Encoded bits | Role |
| --- | --- | --- | --- |
| funct7 | 31..25 | 0000000 | Select ADD within the register group. |
| rs2 | 24..20 | 00011 | Select x3. |
| rs1 | 19..15 | 00010 | Select x2. |
| funct3 | 14..12 | 000 | Part of operation selection. |
| rd | 11..7 | 00001 | Select x1. |
| opcode | 6..0 | 0110011 | Register arithmetic/logic group. |

```text
0000000 00011 00010 000 00001 0110011 = 0x003100B3
```

At PC=0x1000 the word is stored as bytes `B3 00 31 00`. After execution:

| State | Before | After |
| --- | --- | --- |
| x1 | 99 | 12 |
| x2 / x3 | 5 / 7 | 5 / 7 |
| PC | 0x1000 | 0x1004 |
| Retired count | 41 | 42 |

Both sources are read before writing rd. Therefore `add x2, x2, x3` is valid:
it adds using the old x2, then replaces x2 with the sum.

`instruction_extract_fields` only extracts raw slices. The decoder selects
ADD using opcode, funct3, and funct7; the CPU reads values and performs the work.

## Worked example: SUB

```asm
sub x1, x2, x3
```

With x2=12 and x3=7, x1 becomes 5. Its word is 0x403100B3. Changing ADD's
funct7 from 0x00 to 0x20 selects SUB.

With x2=5 and x3=7, the result is 0xFFFFFFFE. These same 32 bits represent
-2 as signed two's complement or 4,294,967,294 as unsigned. Registers hold bits;
the instruction determines how comparisons interpret them. ADD/SUB retain the
low 32 bits and do not raise overflow exceptions.

## Worked example: ADDI and sign extension

```asm
addi x5, x0, 12
addi x6, x5, -1
```

The first instruction reads zero from x0 and writes 12 to x5. The second reads
12 from x5 and adds the constant -1 from the instruction, producing 11 in x6.

An ordinary I-format immediate has 12 bits: -2048 through 2047. The 12-bit -1
is 0xFFF. Sign extension repeats bit 11 into all upper bits, producing
0xFFFFFFFF. This interpretation also applies before the unsigned comparison
in SLTIU; for example `sltiu x1, x2, -1` compares x2 against 0xFFFFFFFF.

## Instruction formats

All instructions in this target occupy four bytes. R, I, S, and U are the base
layouts; B and J rearrange immediate fields for control transfers.

| Format | Fields / purpose | Example |
| --- | --- | --- |
| R | rd, rs1, rs2; two register inputs | `add x1, x2, x3` |
| I | rd, rs1, constant; arithmetic, loads, JALR | `addi x1, x2, 7` |
| S | rs1 base, rs2 data, constant offset | `sw x3, 0(x2)` |
| B | rs1, rs2, PC-relative offset | `beq x1, x2, -8` |
| U | rd, upper 20 bits | `lui x1, 0x12345` |
| J | rd, PC-relative offset | `jal x1, 16` |

Bit layouts, most significant bit first:

```text
R: funct7[6:0]  rs2[4:0]  rs1[4:0]  funct3  rd[4:0]   opcode
I: imm[11:0]              rs1[4:0]  funct3  rd[4:0]   opcode
S: imm[11:5]    rs2[4:0]  rs1[4:0]  funct3  imm[4:0]  opcode
B: imm[12|10:5] rs2[4:0]  rs1[4:0]  funct3  imm[4:1|11] opcode
U: imm[31:12]                              rd[4:0]   opcode
J: imm[20|10:1|11|19:12]                    rd[4:0]   opcode
```

S-format splits the 12-bit offset into two parts. B and J omit offset bit zero,
which is always zero. The decoder reconstructs the parts and sign-extends the
result. B spans -4096..4094 bytes; J spans -1,048,576..1,048,574 bytes.
Our target has no compressed instructions, so a taken target must also align to
four bytes. An encoded two-byte offset can therefore produce an alignment trap.

I-format shifts use a five-bit shift amount, 0..31, and additional operation
selector bits. Raw bits 24..20 are not a second register input in ADDI. Always
interpret extracted slices using the decoded format.

LUI puts its 20-bit operand in bits 31..12; the low 12 bits become zero.
`lui x1, 0x12345` writes 0x12345000. AUIPC adds that value to the address
of the AUIPC instruction, retaining the low 32 bits.

## Supported instructions

| Family | Operations | Behavior |
| --- | --- | --- |
| Register arithmetic | ADD, SUB | Modulo-2^32 addition/subtraction. |
| Register shifts | SLL, SRL, SRA | Shift amount uses only the low five bits of rs2. |
| Register comparison | SLT, SLTU | Write 1 when less, otherwise 0; signed/unsigned respectively. |
| Register logic | XOR, OR, AND | Bitwise operations. |
| Immediate arithmetic | ADDI | Add a sign-extended 12-bit constant. |
| Immediate comparison | SLTI, SLTIU | Signed/unsigned comparison against the extended constant. |
| Immediate logic | XORI, ORI, ANDI | Bitwise operation with an extended constant. |
| Immediate shifts | SLLI, SRLI, SRAI | Shift by an encoded amount from 0 through 31. |
| Upper immediate | LUI, AUIPC | Set upper bits, or add them to the current PC. |
| Jumps | JAL, JALR | Change PC and save PC+4 in rd. |
| Equality branches | BEQ, BNE | Branch when equal / different. |
| Signed branches | BLT, BGE | Branch when less / greater or equal. |
| Unsigned branches | BLTU, BGEU | Same comparisons treating inputs as unsigned. |
| Signed loads | LB, LH | Read 8 / 16 bits and sign-extend to 32. |
| Other loads | LW, LBU, LHU | Read 32 bits, or zero-extend 8 / 16 bits. |
| Stores | SB, SH, SW | Write the low 8 / 16 / 32 bits of rs2. |
| Ordering | FENCE | Memory is already sequential in this machine. |
| Environment | ECALL, EBREAK | Report an environment-call / breakpoint trap. |

SRA/SRAI fill high bits with the old sign bit. SRL/SRLI fill them with zero.
For example, shifting 0x80000000 right by one produces 0xC0000000 arithmetically
or 0x40000000 logically.

x0 reads as zero; writes discard their value. Discarding a load's destination
does not suppress memory access or its potential faults.

## RAM, loads, and stores

RAM defaults to 64 KiB: addresses 0x00000000 through 0x0000FFFF. Select another
capacity with `--ram SIZE`, using B, KiB, or MiB. The monitor accepts 4 bytes
through 256 MiB in multiples of four. A 32-bit byte address can name 4 GiB;
it does not require that much installed RAM. The current cap is a resource policy.

The following last-address examples use the default 64 KiB. At other capacities,
the last valid start is capacity minus the access width.

`lw x9, 0(x8)` computes address = x8 + signed offset, modulo 2^32, then reads
four bytes. `sw x7, 0(x8)` computes the same kind of address and writes x7.

| Width | Alignment | Last valid start |
| --- | --- | --- |
| Byte | Any address | 0xFFFF |
| Halfword | Multiple of 2 | 0xFFFE |
| Word | Multiple of 4 | 0xFFFC |

The whole access must fit. Stores validate before changing bytes. In this
execution environment, misalignment is diagnosed before an out-of-range check.
No rejected instruction partially updates RAM or registers.

Little-endian example: SW of 0xFEDCBA98 writes `98 BA DC FE` at ascending
addresses. An LB of the first byte produces 0xFFFFFF98, whereas LBU produces
0x00000098. Code and data share RAM; there are no page permissions or devices.

## Branches, jumps, and functions

Branches add their signed offset to the branch instruction's address, not PC+4.
An untaken branch proceeds to PC+4. Only a taken branch checks target alignment.

JAL writes PC+4 into rd and sets PC to the instruction address plus the offset.
JALR writes PC+4 and uses rs1 + offset as its target, clearing bit zero first.
A remaining bit-one misalignment traps before writing the return address.
When rd equals rs1, the target uses the old register value.

`jal x1, offset` conventionally calls a function. `jalr x0, 0(x1)` returns
without retaining another return address. Names ra=x1, sp=x2, a0=x10, and a7=x17
are ABI conventions; the CPU still sees numbered registers. The call demo sets
its own stack pointer, saves a return address on the stack, and restores it.

An aligned jump outside RAM retires normally. The following instruction fetch
faults at the new address. A misaligned target instead faults at the branch or
jump that tried to select it.

## Execution environment

The simulator reports traps to its caller; it does not implement privilege
levels, trap-vector dispatch, or CSR registers.

| Condition | Cause number | Diagnostic value |
| --- | --- | --- |
| Misaligned instruction address | 0 | Bad PC or taken target |
| Instruction access fault | 1 | Fetch address |
| Illegal / unsupported instruction | 2 | Instruction word |
| EBREAK | 3 | Breakpoint instruction address |
| Misaligned load | 4 | Data address |
| Load access fault | 5 | Data address |
| Misaligned store | 6 | Data address |
| Store access fault | 7 | Data address |
| ECALL | 8 | Zero; teaching convention uses the user ECALL cause |

For every trap, `instruction_address` is the PC of the instruction being
attempted. The original CPU/RAM state, PC, and retirement counter are preserved.

The controller implements one service: put 93 in a7 and an exit status in a0,
then execute ECALL. This convention resembles a Linux exit service, but there
is no Linux operating system or other syscall support. The core still reports
ECALL as a trap; the controller records a successful program exit. ECALL does
not retire, which explains 24 attempts and 23 retired instructions in the sum
demo. Repeated step/run after exit does nothing until reset.

EBREAK pauses at its own address and does not retire. Repeating step will
encounter it again; use reset to restart. This monitor does not implement
breakpoint removal or automatic resume beyond EBREAK.

FENCE retires as a no-op because all memory effects are already applied in
order. Reserved FENCE rd/rs1 and ordering fields are ignored conservatively,
as required for the base instruction. There is no delayed I/O or multiple-hart
ordering model.

## Step results

`CPU_STEP_OK` means one instruction retired. Other results distinguish halted
state, invalid API arguments, instruction alignment/access, illegal words,
load/store alignment/access, ECALL, and EBREAK. `cpu_step_result_name` provides
their descriptions. Invalid arguments and an already halted CPU do not produce
an architectural trap.

A rejected CPU step does not set `halted`. The controller decides whether to
resume or stop. The 64-bit retirement counter wraps modulo 2^64 and is not a
clock-cycle counter.

## Step record contract

`cpu_step_recorded` optionally fills a `CpuStepRecord`. The record storage
must be separate from CPU/RAM and other input state.

| Fields | Interpretation |
| --- | --- |
| result | CPU step result, even for invalid arguments. |
| pc_before / pc_after | Address before and after the attempt. |
| count_before / count_after | Retirement counter before and after. |
| fetched / instruction | Whether a word was fetched; decoded kind, format, raw fields, and immediate. |
| rs1_value / rs2_value | Values read from raw register slices; only actual operands are meaningful. |
| branch_taken | A completed taken branch or jump. |
| reg.written | A destination write was attempted, including a discarded x0 write. |
| reg.value | Computed destination value before the x0 rule. |
| reg.before / after / changed | Architectural values and whether they differ. |
| memory.attempted / completed | Whether a data access was requested and succeeded. |
| memory.write / width / address | Direction, width in bytes, and effective address. |
| memory.before / after | Raw memory value before/after a completed access; equal for loads. |
| trap.raised / cause / instruction_address / value | Precise trap information. |

Use flags before reading conditional details. A successful write can leave a
register unchanged; x0 always remains zero. Load sign extension is reflected in
the register result, not in the raw memory values. Records capture one attempt;
the monitor does not retain a reversible execution history.

## Commands and file format

| Command | Effect |
| --- | --- |
| step [N] | Execute up to N instructions, showing effects; default 1. |
| run [N] | Execute until a stop or a maximum number of attempts. |
| stop | Mark CPU halted at the prompt. A later step/run resumes. |
| reset | Restore all initial RAM, clear registers/count/exit state, and restore entry PC. |
| regs | Show register aliases, hexadecimal/signed/unsigned values, PC, and count. |
| mem ADDRESS [BYTES] | Inspect 1..4096 bytes, default 32. |
| disasm ADDRESS [COUNT] | Inspect 1..256 aligned words, default 8. |
| where [ADDRESS] | Show source location and text; default current PC. |
| sources | List recorded source file indices and paths. |
| source [LINE] [FILE_INDEX] | Inspect source context; mark rows with no mapped instruction. |
| symbols [NAME] | List ELF symbols or find an exact name. |
| help [formats\|runtime\|source\|MNEMONIC] | Show commands or contextual information. |
| quit | Leave the monitor. |

The default run limit is 100,000 attempts. `--max-steps N` changes the default;
an explicit interactive `run N` or `step N` overrides it for that command.
Ctrl-C requests a stop at the next instruction boundary. A limit pauses without
discarding state. Resuming continues from the current PC. Inspection is read-only.

Command numbers use decimal or a 0x hexadecimal prefix. Leading zeroes in a
decimal number do not select octal. Negative, overflowing, and malformed numbers
are rejected. For text words, `--entry` defaults to `--load-address`. Raw binary
requires both explicitly. ELF provides its own addresses. The initial entry
must point to a complete aligned instruction in the loaded program.

A `.words` file is a teaching input format:

```text
# One 32-bit hexadecimal word per line. Optional 0x prefix.
00C00293 # addi x5, x0, 12
05D00893 # addi a7, x0, 93
00000073 # ecall; a0 is initially zero
```

Blank lines, CRLF, a final line without newline, and # comments are accepted.
A line is limited to 511 bytes before the newline. NUL bytes, extra tokens,
empty programs, oversized words/images, and invalid addresses are rejected.
The full image is validated before replacing the loaded state. RAM outside the
image starts at zero.

This format is distinct from an assembler source, Intel HEX, ELF, or a raw
binary. ELF and raw binary are loaded with `--elf` and `--bin` respectively;
see [C workflow](C_WORKFLOW.md). Disassembly is for inspection; the displayed FENCE ordering fields are descriptive output.

In batch mode, shell status is a0 & 255 for program exit, 1 for a trap, 2 for an
input/setup error, 124 for reaching the limit, and 130 for Ctrl-C. The displayed
32-bit exit value retains all a0 bits. Use the diagnostic text to distinguish a
program-chosen nonzero exit from a monitor error.

## C source organization

Public headers contain declarations and use `#pragma once`. Function bodies
with external linkage belong in exactly one implementation file. Defining one
in a header can cause linker multiple-definition errors even with pragma once.

C identifiers are case-sensitive: `instruction_extract_fields` and
`instruction_Extract_fields` are different symbols. A declaration, definition,
and call must agree.

## References

- [RV32I specification](https://docs.riscv.org/reference/isa/v20260120/unpriv/rv32.html)
- [Instruction encodings](https://github.com/riscv/riscv-opcodes/blob/master/extensions/rv_i)
- [RISC-V field positions](https://github.com/riscv/riscv-opcodes/blob/master/arg_lut.csv)
