/*
  © 2026 Intel Corporation

  This software and the related documents are Intel copyrighted materials, and
  your use of them is governed by the express license under which they were
  provided to you ("License"). Unless the License provides otherwise, you may
  not use, modify, copy, publish, distribute, disclose or transmit this software
  or the related documents without Intel's prior written permission.

  This software and the related documents are provided as is, with no express or
  implied warranties, other than those that are expressly stated in the License.
*/

#include <simics/simulator/control.h>
#include "cosim.h"
#include "rv32-helpers.h"

uint32
reg_read_x(rv32_t *cpu, unsigned n)
{
        ASSERT(n < RV32_NUM_REGS);
        return n == 0 ? 0 : cpu->x[n];
}

void
reg_write_x(rv32_t *cpu, unsigned n, uint32 value)
{
        ASSERT(n < RV32_NUM_REGS);
        if (n != 0)
                cpu->x[n] = value;
}

uint8
mem_read8(rv32_t *cpu, uint32 addr)
{
        uint8 byte;
        if (!read_simics_memory(cpu, addr, &byte, 1))
                return 0;
        return byte;
}

void
ebreak(rv32_t *cpu)
{
        rv32_halt(cpu, "ebreak");
}

void
illegal_instruction(rv32_t *cpu, uint32 pc, uint32 instr)
{
        SIM_LOG_ERROR(cpu->obj, 0,
                      "illegal or unsupported instruction 0x%08x at 0x%08x",
                      instr, pc);
        SIM_break_simulation("illegal instruction");
}

void
increment_cycle(rv32_t *cpu)
{
        cosim_core_increment_cycles(cpu->core, 1);
}

void
increment_step(rv32_t *cpu)
{
        cosim_core_increment_steps(cpu->core, 1);
}
