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

#include "cosim.h"
#include "rv32-core.h"
#include "rv32-helpers.h"

#include <stdio.h>
#include <simics/processor-api.h>
#include <simics/simulator/control.h>

rv32_t *
rv32_from_core(cosim_core_t *core)
{
        return core->rv32;
}

logical_address_t
rv32_get_pc(cosim_core_t *core)
{
        return rv32_from_core(core)->pc;
}

void
rv32_set_pc(cosim_core_t *core, logical_address_t pc)
{
        rv32_from_core(core)->pc = (uint32)pc;
}

/*
 * Register table: x0..x31 and pc, visible through pregs, read-reg/write-reg,
 * the int_register interface and one attribute per register.
 */
static uint64
reg_get_x(cosim_core_t *core, int n)
{
        return reg_read_x(rv32_from_core(core), n);
}

static void
reg_set_x(cosim_core_t *core, int n, uint64 value)
{
        reg_write_x(rv32_from_core(core), n, (uint32)value);
}

static uint64
reg_get_pc(cosim_core_t *core, int n)
{
        return rv32_get_pc(core);
}

static void
reg_set_pc(cosim_core_t *core, int n, uint64 value)
{
        rv32_set_pc(core, value);
}

void
rv32_init_class(cosim_core_class_t *core_class)
{
        for (int i = 0; i < RV32_NUM_REGS; i++) {
                char name[8];
                snprintf(name, sizeof name, "x%d", i);
                cosim_core_add_register_declaration(NULL, name, i,
                                                     reg_get_x, reg_set_x, 0);
        }
        cosim_core_add_register_declaration(NULL, "pc", 0,
                                             reg_get_pc, reg_set_pc, 0);
}

rv32_t *
rv32_new_instance(cosim_core_t *core)
{
        rv32_t *cpu = MM_ZALLOC(1, rv32_t);
        cpu->core = core;
        cpu->obj = core->obj;
        return cpu;
}

void
rv32_delete_instance(rv32_t *cpu)
{
        /* Free here what you allocated in rv32_new_instance(). This runs
           when the core object is deleted. */

        MM_FREE(cpu);
}

void
rv32_execute(cosim_core_t *core)
{
        execute(rv32_from_core(core));
}

bool
read_simics_memory(rv32_t *cpu, uint32 addr, uint8 *buf, unsigned len)
{
        /* An instruction-fetch access to the core's physical memory space */
        return cosim_core_fetch_instruction(cpu->core, addr, len, buf, true);
}

void
rv32_halt(rv32_t *cpu, const char *reason)
{
        SIM_LOG_INFO(1, cpu->obj, 0, "halted at pc 0x%08x: %s",
                     cpu->pc, reason);
        SIM_break_simulation(reason);
}
