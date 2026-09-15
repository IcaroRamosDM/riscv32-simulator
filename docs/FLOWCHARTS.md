# Core and Monitor Flowcharts

These diagrams describe implemented stage-2 behavior. One CPU step attempts
one RV32I instruction; the machine controller supplies the execution loop.

## 1. Current system overview

```mermaid
flowchart TD
    FILE["Annotated .words file"] --> LOAD["Validate all words, image bounds, and entry"]
    LOAD --> IMAGE["Initial program image"]
    IMAGE --> MACHINE["Machine: CPU, current RAM, and initial image"]
    USER["Terminal: step, run, stop, reset, inspect"] --> CONTROL["Machine controller"]
    CONTROL --> STEP["cpu_step_recorded"]
    MACHINE --> STEP
    STEP --> FETCH["Fetch word at PC"]
    FETCH --> DECODE["Decode one of 40 RV32I operations"]
    DECODE --> EXEC["Validate and execute"]
    EXEC --> STATE["Commit successful CPU/RAM effects"]
    EXEC --> TRAP["Or preserve state and report a trap"]
    STATE --> RECORD["Step record: instruction, before/after values, result"]
    TRAP --> RECORD
    RECORD --> CONTROL
    CONTROL --> DISPLAY["Terminal displays execution and stop reason"]
```

CPU execution does not print or depend on the terminal. The same records can
later drive graphical views. The current input is machine words; compiling
user C and loading ELF/debug information are stage 3.

## 2. Reset and program lifecycle

```mermaid
flowchart TD
    START["Open input file"] --> TEMP["Parse into a temporary zero-filled image"]
    TEMP --> VALID{"All words and addresses valid?"}
    VALID -->|No| ERROR["Report file and line; preserve previous image"]
    VALID -->|Yes| KEEP["Keep initial image and entry"]
    KEEP --> RESET["machine_reset"]
    RESET --> RAM["Restore all RAM from initial image"]
    RAM --> CPU["Zero registers and retired count"]
    CPU --> PC["Set PC to entry; clear halted and exit state"]
    PC --> READY["Ready to step or run"]
    USER["User requests reset"] --> RESET
```

The lower-level `cpu_reset` and `memory_reset` are independent operations.
CPU reset alone does not erase RAM; RAM reset alone does not change CPU.
Machine reset intentionally restores both to the loaded-program starting point.

## 3. Register access

```mermaid
flowchart TD
    START["Register read or write"] --> VALID{"Valid required pointers and index 0..31?"}
    VALID -->|No| FAIL["Return false; preserve state and output"]
    VALID -->|Yes| OP{"Read or write?"}
    OP -->|Read| RZERO{"x0?"}
    RZERO -->|Yes| ZERO["Return value zero"]
    RZERO -->|No| READ["Return selected register value"]
    OP -->|Write| WZERO{"x0?"}
    WZERO -->|Yes| DISCARD["Discard computed value"]
    WZERO -->|No| WRITE["Replace destination value"]
    ZERO --> OK["Return true"]
    READ --> OK
    DISCARD --> OK
    WRITE --> OK
```

Reading a register selects its stored value, not its register number.
All source values are read before a destination write, so rd may overlap rs1/rs2.

## 4. Loads and stores

```mermaid
flowchart TD
    START["Decode load/store width and signed offset"] --> ADDRESS["Address = old rs1 + offset, low 32 bits"]
    ADDRESS --> ALIGN{"Address aligned to width?"}
    ALIGN -->|No| MISALIGN["Load/store misalignment trap"]
    ALIGN -->|Yes| RANGE{"Entire access inside RAM?"}
    RANGE -->|No| ACCESS["Load/store access trap"]
    RANGE -->|Yes| KIND{"Load or store?"}
    KIND -->|Load| READ["Read bytes in little-endian order"]
    READ --> EXTEND["LB/LH: sign extension; LBU/LHU: zero extension"]
    EXTEND --> REG["Write rd, or discard for x0"]
    KIND -->|Store| OLD["Capture previous memory value"]
    OLD --> WRITE["Write low 8, 16, or 32 bits of old rs2"]
    REG --> COMMIT["Advance PC and increment retired count"]
    WRITE --> COMMIT
    MISALIGN --> FAIL["Preserve CPU/RAM; report address and faulting PC"]
    ACCESS --> FAIL
```

For width 1, 2, or 4, the range test uses
`address <= MEMORY_SIZE - width`. It does not overflow by adding the width.
Load/store execution diagnoses misalignment first; the underlying memory API
returns a single false result for either alignment or range failure.

Even a load to x0 follows the memory-validation path.

## 5. Instruction fetch

```mermaid
flowchart TD
    PC["Read current PC"] --> ALIGN{"PC is a multiple of 4?"}
    ALIGN -->|No| MISALIGN["Instruction alignment trap"]
    ALIGN -->|Yes| RANGE{"Four bytes fit inside RAM?"}
    RANGE -->|No| ACCESS["Instruction access trap"]
    RANGE -->|Yes| READ["Read bytes PC through PC+3"]
    READ --> WORD["Assemble 32-bit little-endian word"]
    WORD --> DECODE["Decode without changing CPU/RAM"]
```

The public `cpu_fetch_instruction` is read-only, also works while halted, and
returns false without changing its output on failure. The combined step adds
the distinct architectural diagnostics shown here.

## 6. Field extraction and decoding

```mermaid
flowchart TD
    WORD["32-bit word"] --> RAW["Preserve raw word and extract bit slices"]
    RAW --> OPCODE["Use opcode to select instruction family and format"]
    OPCODE --> SELECT["Check required funct3/funct7 or exact system encoding"]
    SELECT --> VALID{"Supported RV32I encoding?"}
    VALID -->|No| UNKNOWN["Unknown kind; preserve original word"]
    VALID -->|Yes| IMM["Reassemble and extend any immediate"]
    IMM --> DECODED["Operation, format, raw fields, and immediate"]
```

R uses register operands. I embeds a constant. S splits an offset around other
fields. B/J rearrange offset bits and imply a zero low bit. U provides upper
bits. A raw slice named rs2 is not a register operand in every format.

## 7. Integer operations

```mermaid
flowchart TD
    START["Decoded arithmetic, logic, comparison, or shift"] --> LEFT["Read old rs1"]
    LEFT --> RIGHT{"Second operand source?"}
    RIGHT -->|R format| REG["Read old rs2"]
    RIGHT -->|I format| IMM["Use sign-extended immediate or five-bit shift amount"]
    REG --> OP["Apply decoded operation"]
    IMM --> OP
    OP --> RESULT["32-bit result; comparison writes 0 or 1"]
    RESULT --> DEST{"rd is x0?"}
    DEST -->|Yes| DISCARD["Record computed value, preserve x0"]
    DEST -->|No| WRITE["Record before/after; update rd"]
    DISCARD --> NEXT["PC+4; retired count+1"]
    WRITE --> NEXT
```

For example, ADDI reads x0=0 and the encoded constant 12, producing x5=12.
ADD then reads its two selected registers. SUB can produce negative bit
patterns; it does not raise an overflow exception.

LUI uses only its upper immediate. AUIPC adds that value to its own instruction
address. Neither consumes rs1/rs2 as meaningful operands.

## 8. Branches, jumps, and fault attribution

```mermaid
flowchart TD
    START["Branch or jump"] --> KIND{"Conditional branch?"}
    KIND -->|Yes| COMPARE["Compare old source values as specified"]
    COMPARE --> TAKEN{"Condition true?"}
    TAKEN -->|No| FALL["Next PC = old PC + 4"]
    TAKEN -->|Yes| REL["Target = old PC + signed offset"]
    KIND -->|No| JUMP{"JAL or JALR?"}
    JUMP -->|JAL| REL
    JUMP -->|JALR| INDIRECT["Target = old rs1 + offset; clear bit zero"]
    REL --> ALIGN{"Target is a multiple of 4?"}
    INDIRECT --> ALIGN
    ALIGN -->|No| TRAP["Trap at source instruction; preserve link register and PC"]
    ALIGN -->|Yes| LINK{"Jump saves a link?"}
    LINK -->|Yes| SAVE["Write old PC+4 to rd; x0 discards"]
    LINK -->|No| TARGET["Select target as next PC"]
    SAVE --> TARGET
    TARGET --> COUNT["Commit PC; increment retired count"]
    FALL --> COUNT
```

Alignment of an untaken branch target is irrelevant. An aligned target outside
RAM is accepted by the control-transfer instruction; fetching there faults
on the next step.

## 9. Single-instruction execution

```mermaid
flowchart TD
    START["cpu_step_recorded: snapshot PC/count"] --> PTR{"Required pointers valid?"}
    PTR -->|No| ARG["Invalid-argument result"]
    PTR -->|Yes| HALT{"Already halted?"}
    HALT -->|Yes| STOP["Halted result"]
    HALT -->|No| FETCH["Validate PC and fetch word"]
    FETCH --> FOK{"Fetch succeeded?"}
    FOK -->|No| FAULT["Record precise trap"]
    FOK -->|Yes| DECODE["Decode operation and operands"]
    DECODE --> KNOWN{"Supported operation?"}
    KNOWN -->|No| FAULT
    KNOWN -->|Yes| PREPARE["Read old operands; calculate candidate effects"]
    PREPARE --> VALID{"No trap or access/target failure?"}
    VALID -->|No| FAULT
    VALID -->|Yes| COMMIT["Commit register or memory effect, then PC and count"]
    COMMIT --> OK["OK result and before/after record"]
    FAULT --> PRESERVE["Return with CPU/RAM unchanged"]
    ARG --> PRESERVE
    STOP --> PRESERVE
```

No operation has a fallible action after it modifies architectural state.
The existing flat RAM makes a checked store complete synchronously. The
counter measures successful instructions, not clock cycles. ECALL and EBREAK
take the trap path and do not increment it.

## 10. Monitor execution loop

```mermaid
flowchart TD
    COMMAND["step N or run N"] --> STATE{"Program loaded and not already exited?"}
    STATE -->|No| RETURN["Report current stop reason"]
    STATE -->|Yes| STOP{"Ctrl-C requested?"}
    STOP -->|Yes| HALT["Set halted; report stopped"]
    STOP -->|No| STEP["Attempt one CPU instruction"]
    STEP --> RECORD["Capture step record; display when requested"]
    RECORD --> RESULT{"CPU result?"}
    RESULT -->|OK| LIMIT{"Attempt limit reached?"}
    LIMIT -->|No| STOP
    LIMIT -->|Yes| PAUSE["Report limit; preserve current state"]
    RESULT -->|ECALL| SERVICE{"a7 equals 93?"}
    SERVICE -->|Yes| EXIT["Record program exit using a0"]
    SERVICE -->|No| TRAP["Report unsupported service or trap"]
    RESULT -->|Other non-OK| TRAP
```

`step` defaults to one attempt and always displays effects. `run` uses the
configured maximum. Ctrl-C is checked between attempts. A later run resumes
a manual stop or limit; an exited program requires reset.

## 11. Sum demo

```mermaid
flowchart TD
    INIT["x5=1, x6=6, x7=0"] --> ADD["x7 = x7 + x5"]
    ADD --> INC["x5 = x5 + 1"]
    INC --> BRANCH{"x5 less than x6?"}
    BRANCH -->|Yes| ADD
    BRANCH -->|No| STORE["SW: write x7=15 to RAM at 0x100"]
    STORE --> LOAD["LW: read 15 into x9"]
    LOAD --> EXIT["a0=0, a7=93, ECALL"]
```

The loop adds 1, 2, 3, 4, and 5. After the last addition, x5 becomes 6 and the
branch falls through. The word 15 appears in memory as `0F 00 00 00`.

## 12. Function call and stack demo

```mermaid
flowchart TD
    START["Set sp=0x10000 and a0=7"] --> CALL["JAL at 0x08: ra=0x0C, target=0x1C"]
    CALL --> FRAME["Subtract 16 from sp"]
    FRAME --> SAVE["Save ra at sp+12 and a0 at sp+8"]
    SAVE --> DOUBLE["Double a0: 7+7=14"]
    DOUBLE --> RESTORE["Restore ra; add 16 to sp"]
    RESTORE --> RETURN["JALR x0, 0(ra): return to 0x0C"]
    RETURN --> KEEP["Copy returned 14 into x5"]
    KEEP --> EXIT["Exit with a0=0 and a7=93"]
```

The stack pointer initially names the byte just above RAM; subtracting 16
creates a valid frame before any stack access. The final sp is restored to
0x10000, x5 contains 14, and saved bytes remain available for inspection.
