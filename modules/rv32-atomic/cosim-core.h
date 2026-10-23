/*
  cosim-core.h - the Simics core object for the RV32I course core

  © 2026 Intel Corporation

  This software and the related documents are Intel copyrighted materials, and
  your use of them is governed by the express license under which they were
  provided to you ("License"). Unless the License provides otherwise, you may
  not use, modify, copy, publish, distribute, disclose or transmit this software
  or the related documents without Intel's prior written permission.

  This software and the related documents are provided as is, with no express or
  implied warranties, other than those that are expressly stated in the License.
*/

#ifndef COSIM_CORE_H
#define COSIM_CORE_H

#ifndef COSIM_HEADER
#define COSIM_HEADER "cosim.h"
#endif
#include COSIM_HEADER
#include <simics/processor-api.h>
#include <simics/devs/memory-space.h>
#include <simics/model-iface/breakpoints.h>
#include "cosim-local.h"
#include "event-queue.h"
#include "cosim-exec.h"

/* Registers are handled by creating (at initialization time) a
 * table of registers.  Each table entry has a string name, and
 * an internal register number (in case we have an array of
 * registers, like the gpr or fpr or vmx or ...), as well as two
 * functions -- one to get the register value and the other to
 * set the register value.
 *
 * The Simics register name is the "name"; the Simics register
 * number is the index into the table of registers.  We implement
 * the register table with the Simics Vector type.
 */

typedef short cosim_core_reg_number_t;
typedef uint64 (*reg_get_function_ptr)(cosim_core_t *core, int n);
typedef void (*reg_set_function_ptr)(cosim_core_t *core,
                                     int n, uint64 value);

typedef struct register_description {
        const char          *name;
        int                 reg_number;
        reg_get_function_ptr   get_reg_value;
        reg_set_function_ptr   set_reg_value;
        int                 catchable;
} register_description_t;

typedef VECT(register_description_t) register_table;

/* The core class -- one per core */
typedef struct cosim_core_class {
        conf_class_t *conf_class;
        /* The list of registers for this class of cores */
        register_table reg_table;
} cosim_core_class_t;

/* The core object -- one per thread */
struct cosim_core {
        /* pointer to the corresponding Simics object */
        conf_object_t *obj;

        cosim_core_class_t *core_class;

        conf_object_t *cosim;

        conf_object_t *current_context;
        const breakpoint_query_v2_interface_t *context_bp_query_iface;
        const breakpoint_trigger_interface_t *context_bp_trig_iface;

        conf_object_t *phys_mem_obj;
        const memory_space_interface_t *phys_mem_space_iface;
#ifdef LOCAL_COSIM_CORE_MEMBERS
        LOCAL_COSIM_CORE_MEMBERS
#endif

        int thread_id;
        bool is_enabled;
        double freq_mhz;

        cycles_t idle_cycles;

        struct rv32 *rv32;  /* RV32I state, see rv32-core.h */

        bool issue_stallable_memops;
};

/*
 * Services provided to the cosim
 */
/* global initialization for cores -- called only once */
/* defines the class instance, interfaces and APIs */
void init_cosim_core_class();

/* get the current program counter for the core */
logical_address_t cosim_core_get_pc(cosim_core_t *core);

/* set the current program counter for the core */
void cosim_core_set_pc(cosim_core_t *core, logical_address_t value);

/* convert a logical address to physical address */
int cosim_core_logical_to_physical(cosim_core_t *core,
                                    logical_address_t la_addr,
                                    access_t access_type,
                                    physical_address_t *pa_addr);

/* disassemble a string into (a) a boolean success code,
   and (b) a string (if successful) */
tuple_int_string_t cosim_core_disassemble(conf_object_t *obj,
                                           generic_address_t address,
                                           attr_value_t instruction_data,
                                           int sub_operation);

bool cosim_core_fetch_and_execute_instruction(cosim_core_t *core);

/*
 * Services provided to the processor
 */
/* to declare a register */
/* registers have to be declared for each core */
int cosim_core_add_register_declaration(cosim_core_t *core,
                                         const char *name,
                                         int reg_number,
                                         reg_get_function_ptr get,
                                         reg_set_function_ptr set,
                                         int catchable);

/* check a virtual breakpoint */
void cosim_core_check_virtual_breakpoints(cosim_core_t *core,
                                           access_t access,
                                           logical_address_t virt_start,
                                           generic_address_t len,
                                           uint8 *data);

/* fetch an instruction from memory */
bool cosim_core_fetch_instruction(cosim_core_t *core,
                                   physical_address_t pa,
                                   physical_address_t len,
                                   uint8 *data,
                                   int check_bp);

/* read and write generally */
bool cosim_core_write_memory(cosim_core_t *core,
                              physical_address_t pa,
                              physical_address_t len,
                              uint8 *data,
                              int check_bp);

bool cosim_core_read_memory(cosim_core_t *core,
                             physical_address_t pa,
                             physical_address_t len,
                             uint8 *data,
                             int check_bp);

/* get the running/stopped state of the core */
execute_state_t cosim_core_state(cosim_core_t *core);

/* increment the number of steps, cycles, or check the next event */
void cosim_core_increment_cycles(cosim_core_t *core, int n);
void cosim_core_increment_steps(cosim_core_t *core, int n);
int  cosim_core_next_cycle_event(cosim_core_t *core);
int  cosim_core_next_step_event(cosim_core_t *core);

static inline conf_object_t *
cosim_core_to_conf_obj(cosim_core_t *cosim)
{
        return cosim->obj;
}

static inline cosim_core_t *
conf_obj_to_cosim_core(conf_object_t *obj)
{
        return (cosim_core_t *) SIM_object_data(obj);
}

#endif
