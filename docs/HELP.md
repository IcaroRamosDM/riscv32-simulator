# Learning Guide

## Help available now

From the repository root:

```bash
make help
less docs/HELP.md
```

In `less`, press `q` to exit. The application command-line interface and graphical
help are planned features. The current help consists of build help and this guide.

## Essential terms

| Term | Meaning |
| --- | --- |
| Register | A numbered storage location inside the simulated CPU. |
| Register number | Which register to select, such as 2 for `x2`. |
| Register value | The 32-bit data currently stored in that register. |
| PC | The address of the current instruction. |
| Instruction | An encoded request for an operation. |
| Opcode | A field used to identify the instruction group. |
| Immediate | A constant encoded in the instruction itself. |
| Format | The arrangement of fields within an instruction. |

## Worked example: ADD

Suppose the register contents before execution are:

| Register | Value |
| --- | --- |
| `x1` | 99 |
| `x2` | 5 |
| `x3` | 7 |

The assembly instruction is:

```asm
add x1, x2, x3
```

Read it as: add the value in x2 to the value in x3, then store the result in x1.
The destination comes first in this syntax. In this example, executing ADD would
replace the value 99 in x1 with 12. The source values remain 5 and 7.

The numbers 1, 2, and 3 select registers. The values 5 and 7 are read from CPU
state when the instruction executes. Changing those stored values does not
change the encoding of this instruction.

### From the request to bits

For this R-format instruction, the encoded fields are:

| Field | Bits | Encoded value | Role in this example |
| --- | --- | --- | --- |
| `funct7` | 31-25 | `0000000` | Part of the ADD operation selection |
| `rs2` | 24-20 | `00011` | Select x3 |
| `rs1` | 19-15 | `00010` | Select x2 |
| `funct3` | 14-12 | `000` | Part of the ADD operation selection |
| `rd` | 11-7 | `00001` | Select x1 |
| `opcode` | 6-0 | `0110011` | Register arithmetic/logic group |

In order from bit 31 to bit 0:

```text
0000000 00011 00010 000 00001 0110011
```

These 32 bits are `0x003100B3` in hexadecimal. The opcode, funct3, and funct7
together identify ADD for this encoding; the opcode alone is insufficient.

In our little-endian RAM, this word occupies four bytes in ascending addresses:

```text
B3 00 31 00
```

### What field extraction does

`instruction_extract_fields(0x003100B3)` returns the original word and the six
field values. For example, `rd = 1`, `rs1 = 2`, and `rs2 = 3`.

To extract rd, shifting the word right by 7 places moves bits 11-7 to bits 4-0.
The mask `0x1F` keeps only those five bits. Their binary value `00001` is 1.

The implemented extraction function does not read the values 5 and 7, perform
the addition, or write 12 into x1. Execution is the next development stage.

### What operation identification does

`instruction_decode(0x003100B3)` returns `INSTRUCTION_ADD` and the extracted
fields. It checks all three operation selectors: opcode must be `0x33`, funct3
must be zero, and funct7 must be zero. Changing funct7 to `0x20` produces SUB,
which the current decoder reports as `INSTRUCTION_UNKNOWN`.

Unknown means unrecognized by the current implementation. It does not by itself
prove that the word is invalid RV32I. Decoding preserves the raw word and fields
for unknown operations too, and does not access CPU or RAM state.

## Instruction formats

All formats below describe layouts of a 32-bit instruction in our RV32I target.
The base layouts are R, I, S, and U; B and J are immediate-encoding variants.

| Format | Typical use | Example |
| --- | --- | --- |
| R | Two source registers and a destination | `add x1, x2, x3` |
| I | An immediate constant; also used for loads and JALR | `addi x1, x2, 7` |
| S | Store register data into memory | `sw x3, 0(x2)` |
| B | Branch if a comparison is true | `beq x1, x2, loop` |
| U | Place an immediate in the upper bits | `lui x1, 0x12345` |
| J | Jump and save a return address | `jal x1, function` |

In `addi x1, x2, 7`, the number 7 is an immediate value inside the instruction.
In `add x1, x2, x3`, the second source value comes from register x3.

Some formats reuse the same bit positions for other purposes. For example,
bits 24-20 belong to the immediate in ADDI; interpreting them as a second source
register would be incorrect. Our extraction structure holds raw bit slices.
As support grows, the decoder will interpret the fields for each operation.

## Configurable RAM

The current implementation provides a fixed 64 KiB region. Configurable capacity
is approved planned work, including explicit size units, allocation and bounds
checks, program-fit validation, and saving the choice in project settings.

## Planned application help

- Command-line `--help` for commands, options, and examples.
- A searchable help section accessible through the desktop Help menu and F1.
- Contextual help for the selected instruction, register, memory address, or error.
- A glossary covering PC, registers, values, immediates, formats, and byte order.
- Bit layouts and worked examples for R, I, S, B, U, and J.
- For each supported instruction: syntax, operands, operation, state changes,
  relevant faults, and a flowchart with before/after values.
- Clear indication of implemented features and remaining work.
- Help available offline with the application.

## Linker errors encountered during development

C identifiers are case-sensitive. A declaration, definition, and call must use
the same spelling. `instruction_Extract_fields` and `instruction_extract_fields`
are different names. If callers request one and the object file defines the
other, the linker reports an undefined reference.

A normal externally linked function body belongs in its implementation file.
The header provides its declaration. See [Core Flowcharts](FLOWCHARTS.md) for the
earlier multiple-definition example and the implemented core behavior.

## References

- [RV32I specification](https://docs.riscv.org/reference/isa/v20260120/unpriv/rv32.html)
- [RISC-V operand field positions](https://github.com/riscv/riscv-opcodes/blob/master/arg_lut.csv)
- [Base integer instruction encodings](https://github.com/riscv/riscv-opcodes/blob/master/extensions/rv_i)
