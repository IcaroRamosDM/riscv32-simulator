# Core Flowcharts

This document describes the implemented simulator core. Tests currently act as
the caller: there is no application execution loop yet. Solid connections show
implemented operations or data dependencies. Dashed connections are planned work.

## 1. Current core overview

```mermaid
flowchart TD
    CALL["Caller: independent test programs"] --> INIT["Explicit initialization calls"]
    INIT --> CR["cpu_reset"]
    INIT --> MR["memory_reset"]
    CR --> CPU["CPU state: 32 registers, PC, instruction count, halted"]
    MR --> RAM["RAM: 64 KiB"]
    CALL --> REG["Checked register read or write"]
    REG --> CPU
    CALL --> MEM["Checked RAM access: 8, 16, or 32 bits"]
    MEM --> RAM
    CALL --> FETCH["cpu_fetch_instruction"]
    CPU -->|"PC supplies the address"| FETCH
    RAM -->|"Four validated bytes"| FETCH
    FETCH --> WORD["32-bit word available to the caller"]
    WORD -.-> DEC["Planned: decode the instruction"]
    DEC -.-> EXEC["Planned: execute and update machine state"]
```

CPU and RAM are separate objects. Preparing one does not automatically prepare
the other. The arrows do not imply an automatic processor loop.

## 2. Reset

Both reset functions require a valid object pointer. They are independent calls.

```mermaid
flowchart TD
    CALL["Caller chooses the object to reset"] --> WHICH{"CPU or RAM?"}
    WHICH -->|"CPU"| CPU["cpu_reset"]
    CPU --> REGS["Set all 32 registers to zero"]
    REGS --> STATE["Set PC and instruction count to zero; clear halted"]
    STATE --> CDONE["CPU reset complete; RAM unchanged"]
    WHICH -->|"RAM"| RAM["memory_reset"]
    RAM --> BYTES["Set all 65,536 bytes to zero"]
    BYTES --> MDONE["RAM reset complete; CPU unchanged"]
```

The individual assignments above describe the resulting state; the C
implementation zero-initializes each complete object.

## 3. Register access

A read requires a CPU pointer and a separate writable output pointer. A write
requires a CPU pointer. The functions reject null required pointers and indices
outside 0 through 31.

```mermaid
flowchart TD
    START["Register access request"] --> VALID{"Required pointers non-null and index below 32?"}
    VALID -->|"No"| FAIL["Return false; preserve CPU and output"]
    VALID -->|"Yes"| OP{"Read or write?"}
    OP -->|"Read"| RZERO{"Register x0?"}
    RZERO -->|"Yes"| ZERO["Copy zero to output"]
    RZERO -->|"No"| READ["Copy selected register to output"]
    OP -->|"Write"| WZERO{"Register x0?"}
    WZERO -->|"Yes"| DISCARD["Discard the input value"]
    WZERO -->|"No"| WRITE["Update the selected register"]
    ZERO --> OK["Return true"]
    READ --> OK
    DISCARD --> OK
    WRITE --> OK
```

These operations preserve PC, instruction count, and halted. The access rules
apply through these functions; direct writes to the public structure bypass them.

## 4. RAM access

The selected function fixes the width to 1, 2, or 4 bytes. A read additionally
requires a separate writable output pointer.

| Width | Alignment | Last permitted starting address |
| --- | --- | --- |
| 1 byte | Any byte address | `0x0000FFFF` |
| 2 bytes | Multiple of 2 | `0x0000FFFE` |
| 4 bytes | Multiple of 4 | `0x0000FFFC` |

```mermaid
flowchart TD
    START["RAM request: read or write; width 1, 2, or 4"] --> PTR{"Required pointers non-null?"}
    PTR -->|"No"| FAIL["Return false; preserve RAM and output"]
    PTR -->|"Yes"| RANGE{"Entire access fits inside RAM?"}
    RANGE -->|"No"| FAIL
    RANGE -->|"Yes"| ALIGN{"Address is a multiple of the width?"}
    ALIGN -->|"No"| FAIL
    ALIGN -->|"Yes"| OP{"Read or write?"}
    OP -->|"Read"| READ["Read all bytes and assemble the value in little-endian order"]
    READ --> OUTPUT["Copy the value to output"]
    OP -->|"Write"| WRITE["Split the value into bytes and write in little-endian order"]
    OUTPUT --> OK["Return true"]
    WRITE --> OK
```

For a known valid width, checking `address <= MEMORY_SIZE - width` avoids
overflow in the bounds check. All checks precede memory access. A rejected write
cannot leave a partially updated value. This describes failure behavior in the
current simulator; it is not a thread-synchronization guarantee.

For example, storing `0xFEDCBA98` at address `0x1000` produces:

| Address | Byte |
| --- | --- |
| `0x1000` | `0x98` |
| `0x1001` | `0xBA` |
| `0x1002` | `0xDC` |
| `0x1003` | `0xFE` |

## 5. Instruction fetch

`cpu_fetch_instruction` delegates the memory checks to `memory_read_u32`.
Its output is a raw word; instruction validity is a later decoding decision.

```mermaid
flowchart TD
    START["cpu_fetch_instruction"] --> CPU{"CPU pointer is null?"}
    CPU -->|"Yes"| FAIL["Return false; preserve output"]
    CPU -->|"No"| PC["Use program_counter as the address"]
    PC --> MEM["Call memory_read_u32"]
    MEM --> PTR{"RAM and output pointers non-null?"}
    PTR -->|"No"| FAIL
    PTR -->|"Yes"| RANGE{"PC is at most 0x0000FFFC?"}
    RANGE -->|"No"| FAIL
    RANGE -->|"Yes"| ALIGN{"PC is a multiple of 4?"}
    ALIGN -->|"No"| FAIL
    ALIGN -->|"Yes"| READ["Read bytes at PC, PC+1, PC+2, and PC+3"]
    READ --> ASSEMBLE["Assemble the 32-bit little-endian word"]
    ASSEMBLE --> OUTPUT["Copy the word to instruction"]
    OUTPUT --> OK["Return true"]
```

Fetch preserves registers, PC, instruction count, halted, and RAM. It is available
while halted because it only inspects the current instruction location. Output
storage must be separate from the CPU and RAM state.

Example: with PC equal to `0x1000` and the bytes `13 00 00 80` at that address,
fetch returns `0x80000013`. PC remains `0x1000` and instruction count does not
change. Repeating the call returns the same word while RAM and PC remain unchanged.

## 6. Header and implementation placement

`include/cpu.h` declares the public function with a trailing semicolon.
`src/cpu.c` contains its single function body. Putting a normal externally linked
function body in a header gives each including C source its own definition and
can cause a multiple-definition linker error.

`#pragma once` prevents repeated inclusion within a translation unit; it does
not combine function definitions emitted by different C source files.
