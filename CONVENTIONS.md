<!--
  © 2026 Intel Corporation

  This software and the related documents are Intel copyrighted materials, and
  your use of them is governed by the express license under which they were
  provided to you ("License"). Unless the License provides otherwise, you may
  not use, modify, copy, publish, distribute, disclose or transmit this software
  or the related documents without Intel's prior written permission.

  This software and the related documents are provided as is, with no express or
  implied warranties, other than those that are expressly stated in the License.
-->

# Program file conventions

Programs are text files (`.txt`) in RV32I assembly.

## Format

- One instruction per line. Blank lines are ignored.
- `#` starts a comment that runs to the end of the line.
- Operands are separated by commas. Extra spaces are ignored.
- Registers are `x0` .. `x31`.
- Numbers are decimal or hexadecimal (`0x...`), optionally with a sign:
  `20`, `-8`, `+16`, `0xff`.
- The target of a branch or `jal` is a byte offset from that instruction.
- No labels, no pseudo-instructions.
- The first instruction is placed at the load address, expected to be 0x0.
- `ebreak` ends the program.

## Instructions

| Syntax | Instructions |
| --- | --- |
| `lui rd, imm` | `lui` |
| `auipc rd, imm` | `auipc` |
| `jal rd, offset` | `jal` |
| `jalr rd, offset(rs1)` | `jalr` |
| `beq rs1, rs2, offset` | `beq` `bne` `blt` `bge` `bltu` `bgeu` |
| `addi rd, rs1, imm` | `addi` `slti` `sltiu` `xori` `ori` `andi` |
| `slli rd, rs1, shamt` | `slli` `srli` `srai` |
| `add rd, rs1, rs2` | `add` `sub` `sll` `slt` `sltu` `xor` `srl` `sra` `or` `and` |
| `ebreak` | `ebreak` |

The `imm` of `lui` and `auipc` is the 20-bit upper immediate:
`lui x1, 0x12345` sets `x1` to `0x12345000`.

## Example

```
addi x1, x0, 3        # 0x00
addi x1, x1, -1       # 0x04
bne  x1, x0, -4       # 0x08  jumps to 0x08 - 4 = 0x04
jal  x5, 12           # 0x0c  jumps to 0x0c + 12 = 0x18
addi x2, x0, 1        # 0x10
ebreak                # 0x14
addi x3, x0, 7        # 0x18
jalr x0, 0(x5)        # 0x1c  jumps to x5 + 0 = 0x10
```
