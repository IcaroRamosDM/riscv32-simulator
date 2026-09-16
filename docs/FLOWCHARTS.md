# Core and Monitor Flowcharts

These diagrams describe implemented stages 1 through 3. One CPU step attempts
one RV32I instruction; the machine controller supplies the execution loop.

## 1. Current system overview

```mermaid
flowchart TD
    C["C source"] --> COMPILER["RV32I/ILP32 cross-compiler and linker"]
    COMPILER --> FILE["ELF or raw binary"]
    WORDS["Annotated .words file"] --> LOAD["Validate input, configured RAM, segments, and entry"]
    FILE --> LOAD
    FILE --> DEBUG["ELF symbols and DWARF source ranges"]
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
    DEBUG --> DISPLAY
```

CPU execution does not print or depend on the terminal. The same records can
later drive graphical views. C compilation, ELF/binary loading, and source
mapping are implemented; the desktop interface follows in stage 4.

## 2. Reset and program lifecycle

```mermaid
flowchart TD
    START["Open input file"] --> TEMP["Parse into a temporary zero-filled image"]
    TEMP --> VALID{"Complete input and addresses valid?"}
    VALID -->|No| ERROR["Report input error; preserve previous image"]
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
`address <= memory.size - width`. It does not overflow by adding the width.
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

## C compilation and runtime initialization

```mermaid
flowchart TD
    C["C files and optional Assembly modules"] --> GCC["GCC: RV32I, ILP32, debug information"]
    START["runtime/start.S and memory.c"] --> GCC
    GCC --> LINK["Link with matching libgcc and configured RAM/stack layout"]
    LINK --> FIT{"Static image fits below reserved stack?"}
    FIT -->|No| ERROR["Build diagnostic; keep previous output files"]
    FIT -->|Yes| ARTIFACTS["ELF, raw binary, Assembly listing, map, linker script"]
    ARTIFACTS --> LOAD["Validate and load image into RAM"]
    LOAD --> ENTRY["PC = ELF entry or explicit binary entry"]
    ENTRY --> SP["Startup initializes sp and gp"]
    SP --> BSS["Clear BSS globals"]
    BSS --> MAIN["Call main"]
    MAIN --> EXIT["Return value in a0; a7 = 93; ECALL"]
    EXIT --> STOP["Machine controller reports program exit"]
```

C multiplication and division can call software routines from RV32I libgcc.
The simulated CPU still executes only base RV32I instructions. Startup is part
of the program and is visible in Assembly/source tracing.

## Configurable RAM and ownership

```mermaid
flowchart TD
    OPTION["RAM value with B, KiB, or MiB"] --> SIZE{"4B to 256MiB and multiple of 4?"}
    SIZE -->|No| ERROR["Reject without changing an existing image"]
    SIZE -->|Yes| ALLOC["Allocate temporary zero-filled RAM"]
    ALLOC --> SUCCESS{"Allocation succeeded?"}
    SUCCESS -->|No| ERROR
    SUCCESS -->|Yes| PARSE["Validate and populate the selected input format"]
    PARSE --> VALID{"Whole program valid?"}
    VALID -->|No| FREE["Free temporary storage; preserve previous state"]
    VALID -->|Yes| CLONE["Create independent current and reset RAM images"]
    CLONE --> COPIES{"Both copies succeeded?"}
    COPIES -->|No| FREE
    COPIES -->|Yes| REPLACE["Replace machine only after all work succeeds"]
    REPLACE --> RESET["Reset copies initial RAM into current RAM without allocation"]
    REPLACE --> DESTROY["Destroy frees both owned images"]
```

Memory objects carry their capacity. Every access checks the actual allocation,
not a fixed 64 KiB constant. Reset preserves the selected capacity. During
execution, current RAM and reset RAM do not share their bytes.

## ELF validation and zero-filled memory

```mermaid
flowchart TD
    FILE["Read bounded ELF file into temporary storage"] --> HEADER{"ELF32, little-endian, RISC-V ET_EXEC?"}
    HEADER -->|No| REJECT["Reject and release temporary storage"]
    HEADER -->|Yes| ISA{"Supported ABI, attributes, sections, and tables?"}
    ISA -->|No| REJECT
    ISA -->|Yes| SEGMENTS{"All load segments fit file and RAM, align, and do not overlap?"}
    SEGMENTS -->|No| REJECT
    SEGMENTS -->|Yes| ENTRY{"Entry is aligned and inside file-backed executable bytes?"}
    ENTRY -->|No| REJECT
    ENTRY -->|Yes| RAM["Allocate zero-filled program RAM"]
    RAM --> COPY["Copy each segment's file bytes to its load address"]
    COPY --> ZERO["Remaining segment bytes and gaps stay zero"]
    ZERO --> META["Copy ELF symbols and DWARF line ranges into independent metadata"]
    META --> MACHINE["Install machine state and start at entry"]
```

ELF code/data/stack flags describe the image. This simulator checks the entry's
executable segment but does not enforce page permissions during execution.
Raw binaries use supplied load/entry addresses and have no symbols or DWARF.

## Assembly steps and C source locations

```mermaid
flowchart TD
    STEP["Execute one instruction and create its step record"] --> PC["Use the recorded instruction address"]
    PC --> LOOKUP{"DWARF range covers this address?"}
    LOOKUP -->|No| UNMAPPED["Display: no instruction-to-source mapping"]
    LOOKUP -->|Yes| LOC["Obtain file, line, and column"]
    LOC --> SAME{"Same file and line as previous traced instruction?"}
    SAME -->|Yes| KEEP["Keep the current source label"]
    SAME -->|No| FILE{"Source file available?"}
    FILE -->|No| MISSING["Show path and line; report missing source text"]
    FILE -->|Yes| TEXT["Show the associated source line"]
    KEEP --> EXEC["Display the executed Assembly instruction"]
    MISSING --> EXEC
    TEXT --> EXEC
    UNMAPPED --> EXEC
    EXEC --> CHANGE["Show register/RAM effects and new PC"]
    CHANGE --> STEP
    SOURCE["User opens source context"] --> ROW{"DWARF has any instruction for this row?"}
    ROW -->|No| NONE["Mark: no mapped instruction"]
    ROW -->|Yes| ADDRESS["Show first mapped address; more instructions may follow"]
```

A source label is explanatory metadata. Instruction addresses govern execution.
Blank lines, declarations, compiler transformations, and runtime code prevent a
one-to-one C/Assembly relationship, including at optimization level zero.
