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

/*
  rv32-helpers.h - helper functions for your core.
*/

#ifndef RV32_HELPERS_H
#define RV32_HELPERS_H

#include "rv32-core.h"

/* Read register xN (n = 0..31). Reading x0 always returns 0. */
uint32 reg_read_x(rv32_t *cpu, unsigned n);

/* Write value to register xN (n = 0..31). Writes to x0 are ignored. */
void reg_write_x(rv32_t *cpu, unsigned n, uint32 value);

/* Read the byte at address addr from instruction memory. Returns 0 if
   nothing is mapped at addr (Simics logs the error). */
uint8 mem_read8(rv32_t *cpu, uint32 addr);

/* Stop the simulation because an EBREAK instruction completed: this is how
   programs end. The simulation stops when the current call to execute()
   returns. Leave cpu->pc pointing at the EBREAK. */
void ebreak(rv32_t *cpu);

/* Report that instr, at address pc, is not an instruction your core
   supports: logs an error and stops the simulation. The simulation stops
   when the current call to execute() returns. */
void illegal_instruction(rv32_t *cpu, uint32 pc, uint32 instr);

void increment_cycle(rv32_t *cpu);

void increment_step(rv32_t *cpu);

#endif
