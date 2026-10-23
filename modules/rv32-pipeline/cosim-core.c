/*
  cosim-core.c - the Simics core object for the RV32I course core

  © 2026 Intel Corporation

  This software and the related documents are Intel copyrighted materials, and
  your use of them is governed by the express license under which they were
  provided to you ("License"). Unless the License provides otherwise, you may
  not use, modify, copy, publish, distribute, disclose or transmit this software
  or the related documents without Intel's prior written permission.

  This software and the related documents are provided as is, with no express or
  implied warranties, other than those that are expressly stated in the License.
*/

#include "cosim-core.h"

#include <simics/processor-api.h>

#include <simics/model-iface/exception.h>
#include <simics/model-iface/int-register.h>
#include <simics/model-iface/processor-info.h>

// TODO: should probably move to Processor API
#include <simics/simulator-iface/context-tracker.h>

#include <simics/devs/signal.h>

#include "event-queue.h"
#include "cosim.h"
#include "cosim-memory.h"
#include "cosim-queue.h"
#include "rv32-core.h"

#define STRINGIFY(x) STRINGIFY1(x)
#define STRINGIFY1(x) #x

static cosim_core_class_t *core_class = NULL;

static cosim_t *
associated_queue(cosim_core_t *core)
{
        return conf_obj_to_cosim(core->cosim);
}

static cosim_t *
associated_memory(cosim_core_t *core)
{
        return conf_obj_to_cosim(core->cosim);
}


static register_table *
associated_register_table(cosim_core_t *core)
{
        register_table *rt = &(core->core_class->reg_table);
        return rt;
}

/*
 * cosim attribute functions
 */
static attr_value_t
cosim_core_get_cosim(conf_object_t *obj)
{
        cosim_core_t *core = conf_obj_to_cosim_core(obj);
        return SIM_make_attr_object(core->cosim);
}

static set_error_t
cosim_core_set_cosim(conf_object_t *obj,
                            attr_value_t *val)
{
        cosim_core_t *core = conf_obj_to_cosim_core(obj);
        core->cosim = SIM_attr_object(*val);
        return Sim_Set_Ok;
}

/*
 * physical_memory_space attribute functions
 */
static attr_value_t
cosim_core_get_physical_memory(conf_object_t *obj)
{
        cosim_core_t *core = conf_obj_to_cosim_core(obj);
        return SIM_make_attr_object(core->phys_mem_obj);
}

static set_error_t
cosim_core_set_physical_memory(conf_object_t *obj,
                                attr_value_t *val)
{
        cosim_core_t *core = conf_obj_to_cosim_core(obj);
        const memory_space_interface_t *mem_space_iface;
        conf_object_t *oval = SIM_attr_object(*val);
        mem_space_iface = (memory_space_interface_t *)SIM_c_get_interface(
                oval, MEMORY_SPACE_INTERFACE);
        if (!mem_space_iface)
                return Sim_Set_Interface_Not_Found;

        set_error_t local_err = local_core_set_phys_memory(core, oval);
        if (local_err != Sim_Set_Ok)
                return local_err;

        core->phys_mem_obj = oval;
        core->phys_mem_space_iface = mem_space_iface;

        return Sim_Set_Ok;
}

/*
 * core_enabled attribute functions
 */
static attr_value_t
cosim_core_get_enabled(conf_object_t *obj)
{
        cosim_core_t *core = conf_obj_to_cosim_core(obj);
        return SIM_make_attr_boolean(core->is_enabled);
}


static set_error_t
cosim_core_set_enabled(conf_object_t *obj,
                        attr_value_t *val)
{
        cosim_core_t *core = conf_obj_to_cosim_core(obj);
        core->is_enabled = SIM_attr_boolean(*val);
        return Sim_Set_Ok;
}

/*
 * idle_cycles attribute functions
 */
static attr_value_t
get_idle_cycles(conf_object_t *obj)
{
        cosim_core_t *core = conf_obj_to_cosim_core(obj);
        return SIM_make_attr_uint64(core->idle_cycles);
}

static set_error_t
set_idle_cycles(conf_object_t *obj,
                attr_value_t *val)
{
        cosim_core_t *core = conf_obj_to_cosim_core(obj);
        core->idle_cycles = SIM_attr_integer(*val);
        return Sim_Set_Ok;
}

/*
 * context_handler interface functions
 */
static conf_object_t *
get_current_context(conf_object_t *obj)
{
        cosim_core_t *core = conf_obj_to_cosim_core(obj);
        return core->current_context;
}

static int
set_current_context(conf_object_t *obj, conf_object_t *ctx)
{
        cosim_core_t *core = conf_obj_to_cosim_core(obj);
        ASSERT(ctx);
        core->current_context = ctx;
        core->context_bp_query_iface = SIM_c_get_interface(
                core->current_context, BREAKPOINT_QUERY_V2_INTERFACE);
        core->context_bp_trig_iface = SIM_c_get_interface(
                core->current_context, BREAKPOINT_TRIGGER_INTERFACE);
        ASSERT(core->context_bp_query_iface && core->context_bp_trig_iface);
        return 1;
}

static attr_value_t
cosim_core_get_stallable_memops(conf_object_t *obj)
{
        cosim_core_t *core = conf_obj_to_cosim_core(obj);
        return SIM_make_attr_boolean(core->issue_stallable_memops);
}


static set_error_t
cosim_core_set_stallable_memops(conf_object_t *obj,
                                 attr_value_t *val)
{
        cosim_core_t *core = conf_obj_to_cosim_core(obj);
        core->issue_stallable_memops = SIM_attr_boolean(*val);
        return Sim_Set_Ok;
}

/*
 * processor_info_v2 interface functions
 */
tuple_int_string_t
cosim_core_disassemble(conf_object_t *obj, generic_address_t address,
                        attr_value_t instruction_data, int sub_operation)
{
        cosim_core_t *core = conf_obj_to_cosim_core(obj);

        SIM_LOG_INFO(2, core->obj, 0,
                     "disassembling instruction at address 0x%llx", address);
        if (sub_operation != 0 && sub_operation != -1)
                return (tuple_int_string_t){0, NULL};

        if (SIM_attr_data_size(instruction_data) < 4) {
                /* The opcode is not complete. Tell the caller that we need 4
                   bytes. */
                return (tuple_int_string_t){-4, NULL};
        }

        uint32 instr = UNALIGNED_LOAD_LE32(SIM_attr_data(instruction_data));
        return (tuple_int_string_t){4, rv32_disassemble(instr)};
}

static void
cosim_core_set_program_counter(conf_object_t *obj,
                                logical_address_t pc)
{
        cosim_core_t *core = conf_obj_to_cosim_core(obj);
        SIM_LOG_INFO(2, core->obj, 0,
                     "setting program counter to 0x%llx", pc);
        rv32_set_pc(core, pc);
}

static logical_address_t
cosim_core_get_program_counter(conf_object_t *obj)
{
        cosim_core_t *core = conf_obj_to_cosim_core(obj);
        return cosim_core_get_pc(core);
}


static physical_block_t
cosim_core_logical_to_physical_block(conf_object_t *obj,
                                      logical_address_t address,
                                      access_t access_type)
{
        cosim_core_t *core = conf_obj_to_cosim_core(obj);
        physical_block_t ret;
        ret.valid = cosim_core_logical_to_physical(core, address, access_type,
                                                    &ret.address);
        ret.block_start = ret.address;
        ret.block_end = ret.address;
        SIM_LOG_INFO(2, core->obj, 0,
                     "logical address 0x%llx to physical address 0x%llx",
                     address, ret.address);
        return ret;
}

static processor_mode_t
cosim_core_get_processor_mode(conf_object_t *obj)
{
        return Sim_CPU_Mode_User;  /* we don't have different modes */
}

static int
cosim_core_enable_processor(conf_object_t *obj)
{
        cosim_core_t *core = conf_obj_to_cosim_core(obj);
        if (core->is_enabled)
                return 1;
        core->is_enabled = true;
        return 0;
}

static int
cosim_core_disable_processor(conf_object_t *obj)
{
        cosim_core_t *core = conf_obj_to_cosim_core(obj);
        if (!core->is_enabled)
                return 1;
        core->is_enabled = false;
        return 0;
}

static int
cosim_core_iface_get_enabled(conf_object_t *obj)
{
        cosim_core_t *core = conf_obj_to_cosim_core(obj);
        return core->is_enabled ? 1 : 0;
}

static cpu_endian_t
cosim_core_get_endian(conf_object_t *obj)
{
        return Sim_Endian_Little;
}

static conf_object_t *
cosim_core_get_physical_memory_iface(conf_object_t *obj)
{
        cosim_core_t *core = conf_obj_to_cosim_core(obj);
        return core->phys_mem_obj;
}

static int
cosim_core_get_logical_address_width(conf_object_t *obj)
{
        return 32;
}

static int
cosim_core_get_physical_address_width(conf_object_t *obj)
{
        return 32;
}

static const char *
cosim_core_architecture(conf_object_t *obj)
{
        return "rv32i";
}

/*
 * registers
 */

/* Registers are handled by creating (at initialization time) a
 * table of registers.  Each table entry has a string name, and
 * an internal register number (in case we have an array of
 * registers, like the gpr or fpr or vmx or ...), as well as two
 * functions -- one to get the register value and the other to
 * set the register value.
 *
 * Registers are defined for each core class -- not each core.
 * We assume all cores have the same (set of) registers.  Different
 * instances, so different values; but the same set of registers.
 *
 * The Simics register name is the "name"; the Simics register
 * number is the index into the table of registers.  We implement
 * the register table with the Simics Vector type.
 */

static void
init_register_table(cosim_core_class_t *core_class)
{
        register_table *rt = &(core_class->reg_table);
        VINIT(*rt);
}


static int
search_register_table(cosim_core_class_t *core_class, const char *name)
{
        register_table *rt = &(core_class->reg_table);
        VFORI(*rt, i) {
                register_description_t *rd;
                rd = &VGET(*rt, i);
                if (strcmp(name, rd->name) == 0)
                        return i;
        }

        /* not found in table, return illegal index */
        return -1;
}

int
cosim_core_add_register_declaration(cosim_core_t *core,
                                     const char *name, int reg_number,
                                     reg_get_function_ptr get,
                                     reg_set_function_ptr set,
                                     int catchable)
{
        register_table *rt = &(core_class->reg_table);
        register_description_t *rd;

        /* we wish to define a register "name" */
        /* first see if we already have one */
        int i = search_register_table(core_class, name);

        if (i != -1) {
                rd = &VGET(*rt, i);
        } else {
                /* A new register -- make sure we have space */
                i = VLEN(*rt);
                VRESIZE(*rt, i + 1);
                rd = &VGET(*rt, i);
                /* and remember the name */
                rd->name = MM_STRDUP(name);
        }

        /* now that we have a register, define its properties.
         * at the moment, this is just its read/write functions */
        rd->reg_number = reg_number;
        rd->get_reg_value = get;
        rd->set_reg_value = set;
        rd->catchable = catchable;

        return i;
}

static int
cosim_core_get_register_number(conf_object_t *obj, const char *name)
{
        cosim_core_t *core = conf_obj_to_cosim_core(obj);
        int i = search_register_table(core->core_class, name);

        if (i == -1) {
                SIM_LOG_ERROR(core->obj, 0,
                              "illegal name in get_register_number(%s)", name);
        }
        return i;
}

static const char *
cosim_core_get_register_name(conf_object_t *obj, int reg)
{
        cosim_core_t *core = conf_obj_to_cosim_core(obj);
        register_table *rt = associated_register_table(core);

        if (reg < 0 || reg >= VLEN(*rt)) {
                SIM_LOG_ERROR(core->obj, 0,
                              "illegal register in get_register_name(%d)",
                              reg);
                return NULL;
        }
        register_description_t *rd = &VGET(*rt, reg);
        return rd->name;
}

static uint64
cosim_core_read_register(conf_object_t *obj, int reg)
{
        cosim_core_t *core = conf_obj_to_cosim_core(obj);
        register_table *rt = associated_register_table(core);

        if (reg < 0 || reg >= VLEN(*rt)) {
                SIM_LOG_ERROR(core->obj, 0,
                              "illegal register in read_register(%d)", reg);
                return 0;
        }
        register_description_t *rd = &VGET(*rt, reg);
        return rd->get_reg_value(core, rd->reg_number);
}

static void
cosim_core_write_register(conf_object_t *obj, int reg, uint64 val)
{
        cosim_core_t *core = conf_obj_to_cosim_core(obj);
        register_table *rt = associated_register_table(core);

        if (reg < 0 || reg >= VLEN(*rt)) {
                SIM_LOG_ERROR(core->obj, 0,
                              "illegal register in write_register(%d)", reg);
                return;
        }
        register_description_t *rd = &VGET(*rt, reg);
        rd->set_reg_value(core, rd->reg_number, val);
}


static attr_value_t
cosim_core_all_registers(conf_object_t *obj)
{
        cosim_core_t *core = conf_obj_to_cosim_core(obj);
        register_table *rt = associated_register_table(core);

        /* The cosim_core_all_registers should return a list of
         * integers representing all the registers.  In our case,
         * that is then all the integers from 1 to the number of
         * registers in the register table. */

        attr_value_t ret = SIM_alloc_attr_list(VLEN(*rt));
        VFORI(*rt, i) {
                SIM_attr_list_set_item(&ret, i, SIM_make_attr_uint64(i));
        }
        return ret;
}

static int
cosim_core_register_info(conf_object_t *obj, int reg,
                          ireg_info_t info)
{
        cosim_core_t *core = conf_obj_to_cosim_core(obj);
        register_table *rt = associated_register_table(core);
        if (reg < 0 || reg >= VLEN(*rt)) {
                SIM_LOG_ERROR(core->obj, 0,
                              "illegal register in register_info(reg=%d)",
                              reg);
                return -1;
        }

        switch (info) {
        case Sim_RegInfo_Catchable:
                if (VGET(*rt, reg).catchable)
                        return 1;
                else
                        return 0;
        default:
                SIM_LOG_ERROR(core->obj, 0,
                              "illegal info type in register_info(info=%d)",
                              info);
                return -1;
        }
}

/*
 * exceptions
 */
#define Cosim_Core_Number_Of_Exc 5
static const char *const cosim_core_exception_names[] = {
        "Reset",
        "Data Abort",
        "Interrupt Request",
        "Undefined Instruction",
        "Software Interrupt"
};

static int
cosim_core_get_exception_number(conf_object_t *NOTNULL obj,
                                 const char *NOTNULL name)
{
        for (int i = 0; i < Cosim_Core_Number_Of_Exc; i++) {
                if (strcmp(name, cosim_core_exception_names[i]) == 0)
                        return i;
        }
        return -1;
}

static const char *
cosim_core_get_exception_name(conf_object_t *NOTNULL obj, int exc)
{
        if (exc < 0 || exc >= Cosim_Core_Number_Of_Exc) {
                return NULL;
        }
        return cosim_core_exception_names[exc];
}

static int
cosim_core_get_source(conf_object_t *NOTNULL obj, int exc)
{
        return -1;
}

static attr_value_t
cosim_core_all_exceptions(conf_object_t *NOTNULL obj)
{
        attr_value_t ret = SIM_alloc_attr_list(Cosim_Core_Number_Of_Exc);
        for (unsigned i = 0; i < Cosim_Core_Number_Of_Exc; i++)
                SIM_attr_list_set_item(&ret, i, SIM_make_attr_uint64(i));
        return ret;
}

static void
cosim_core_ext_irq_raise(conf_object_t *obj)
{
        cosim_core_t *core = conf_obj_to_cosim_core(obj);
        SIM_LOG_INFO(2, core->obj, 0,
                     "EXTERNAL_INTERRUPT raised.");
}

static void
cosim_core_ext_irq_lower(conf_object_t *obj)
{
        cosim_core_t *core = conf_obj_to_cosim_core(obj);
        SIM_LOG_INFO(2, core->obj, 0,
                     "EXTERNAL_INTERRUPT lowered.");
}

static void
port_cosim_core_ext_irq_raise(conf_object_t *pobj)
{
        cosim_core_ext_irq_raise(SIM_port_object_parent(pobj));
}

static void
port_cosim_core_ext_irq_lower(conf_object_t *pobj)
{
        cosim_core_ext_irq_lower(SIM_port_object_parent(pobj));
}

static void *
cosim_core_init_object(conf_object_t *obj)
{
        cosim_core_t *core = MM_ZALLOC(1, cosim_core_t);
        core->obj = obj;

        core->core_class = core_class;
        core->is_enabled = true;
        core->rv32 = rv32_new_instance(core);

        return core;
}

static void
cosim_core_finalize(conf_object_t *obj)
{
        // do nothing
}

/* The inverse of cosim_core_init_object(), called when the object is
   deleted */
static void
cosim_core_deinit(conf_object_t *obj)
{
        cosim_core_t *core = conf_obj_to_cosim_core(obj);
        rv32_delete_instance(core->rv32);
        MM_FREE(core);
}


static conf_class_t *
cosim_core_define_class()
{
        return SIM_create_class(
                STRINGIFY(COSIM_CORE_CLASSNAME),
                &(class_info_t){
                  .init = cosim_core_init_object,
                  .finalize = cosim_core_finalize,
                  .deinit = cosim_core_deinit,
                  .short_desc = "RV32I course core",
                  .description = "RV32I core for the course project. Holds the"
                                 " architectural state and runs the student"
                                 " core implementation."
               });
}


static void
cosim_core_register_interfaces(conf_class_t *conf_class)
{
        static const context_handler_interface_t context_handler_iface = {
                .set_current_context = set_current_context,
                .get_current_context = get_current_context
        };
        SIM_register_context_handler(conf_class, &context_handler_iface);

        static const processor_info_v2_interface_t info_iface = {
                .disassemble = cosim_core_disassemble,
                .set_program_counter = cosim_core_set_program_counter,
                .get_program_counter = cosim_core_get_program_counter,
                .logical_to_physical = cosim_core_logical_to_physical_block,
                .get_processor_mode = cosim_core_get_processor_mode,
                .enable_processor = cosim_core_enable_processor,
                .disable_processor = cosim_core_disable_processor,
                .get_enabled = cosim_core_iface_get_enabled,
                .get_endian = cosim_core_get_endian,
                .get_physical_memory = cosim_core_get_physical_memory_iface,
                .get_logical_address_width =
                        cosim_core_get_logical_address_width,
                .get_physical_address_width =
                        cosim_core_get_physical_address_width,
                .architecture = cosim_core_architecture
        };
        SIM_REGISTER_INTERFACE(conf_class, processor_info_v2, &info_iface);
        SIM_register_compatible_interfaces(
                conf_class, PROCESSOR_INFO_V2_INTERFACE);

        static const int_register_interface_t int_iface = {
                .get_number = cosim_core_get_register_number,
                .get_name = cosim_core_get_register_name,
                .read = cosim_core_read_register,
                .write = cosim_core_write_register,
                .all_registers = cosim_core_all_registers,
                .register_info = cosim_core_register_info
        };
        SIM_REGISTER_INTERFACE(conf_class, int_register, &int_iface);

        static const exception_interface_t exc_iface = {
                .get_number = cosim_core_get_exception_number,
                .get_name = cosim_core_get_exception_name,
                .get_source = cosim_core_get_source,
                .all_exceptions = cosim_core_all_exceptions
        };
        SIM_REGISTER_INTERFACE(conf_class, exception, &exc_iface);

        conf_class_t *ext_irq_port = SIM_register_simple_port(
                conf_class, "port.EXTERNAL_INTERRUPT",
                "External interrupt line.");
        static const signal_interface_t port_ext_irq_iface = {
                .signal_raise = port_cosim_core_ext_irq_raise,
                .signal_lower = port_cosim_core_ext_irq_lower
        };
        SIM_REGISTER_INTERFACE(ext_irq_port, signal, &port_ext_irq_iface);

        static const signal_interface_t ext_irq_iface = {
                .signal_raise = cosim_core_ext_irq_raise,
                .signal_lower = cosim_core_ext_irq_lower
        };
        SIM_REGISTER_PORT_INTERFACE(conf_class, signal,
                                    &ext_irq_iface,
                                    "EXTERNAL_INTERRUPT",
                                    "External interrupt line.");
}

static void
cosim_core_register_attributes(conf_class_t *conf_class)
{
        SIM_register_attribute(
                conf_class, "cosim",
                cosim_core_get_cosim,
                cosim_core_set_cosim,
                Sim_Attr_Required,
                "o",
                "Cosimulator object.");

        SIM_register_attribute(
                conf_class, "physical_memory_space",
                cosim_core_get_physical_memory,
                cosim_core_set_physical_memory,
                Sim_Attr_Required,
                "o",
                "Physical memory space.");

        /* Read by the disassemble command */
        SIM_register_attribute(
                conf_class, "physical_memory",
                cosim_core_get_physical_memory,
                NULL,
                Sim_Attr_Pseudo,
                "o",
                "Physical memory space (same as physical_memory_space).");

        SIM_register_attribute(
                conf_class, "core_enabled",
                cosim_core_get_enabled,
                cosim_core_set_enabled,
                Sim_Attr_Optional,
                "b",
                "Core state: enabled.");

        SIM_register_attribute(
                conf_class, "idle_cycles",
                get_idle_cycles,
                set_idle_cycles,
                Sim_Attr_Optional,
                "i",
                "Number of idle cycles.");

        SIM_register_attribute(
                conf_class, "stallable_memops",
                cosim_core_get_stallable_memops,
                cosim_core_set_stallable_memops,
                Sim_Attr_Optional,
                "b",
                "If true, read and write transactions will be issued as"
                " stallable operations.");
}

static attr_value_t
cosim_core_get_register_value(conf_object_t *obj, void *arg)
{
        cosim_core_t *core = conf_obj_to_cosim_core(obj);
        register_description_t *rd;

        ASSERT(arg != NULL);
        rd = (register_description_t *)(arg);

        /* return the value of register i */
        uint64 value = rd->get_reg_value(core, rd->reg_number);
        return SIM_make_attr_uint64(value);
}

static set_error_t
cosim_core_set_register_value(conf_object_t *obj,
                               attr_value_t *val, void *arg)
{
        cosim_core_t *core = conf_obj_to_cosim_core(obj);
        register_description_t *rd;

        ASSERT(arg != NULL);
        rd = (register_description_t *)(arg);
        /* set the value of register i to the "val" parameter */
        uint64 value = SIM_attr_integer(*val);
        rd->set_reg_value(core, rd->reg_number, value);
        return Sim_Set_Ok;
}

static void
cosim_core_register_register_table_attributes(cosim_core_class_t *core_class)
{
        register_table *rt = &(core_class->reg_table);
        VFORI(*rt, i) {
                register_description_t *rd;
                rd = &VGET(*rt, i);

                SIM_register_attribute_with_user_data(
                        core_class->conf_class, rd->name,
                        cosim_core_get_register_value,
                        rd,
                        cosim_core_set_register_value,
                        rd,
                        Sim_Attr_Optional,
                        "i",
                        rd->name);
        }
}

void
init_cosim_core_class()
{
        conf_class_t *conf_class = cosim_core_define_class();
        core_class = MM_ZALLOC(1, cosim_core_class_t);
        core_class->conf_class = conf_class;

        /* initialize the register table */
        /* must be done before we allow the processor to initialize */
        init_register_table(core_class);

        cosim_core_register_attributes(conf_class);
        cosim_core_register_interfaces(conf_class);

        /* give the processor a chance to do any global
           initialization; including defining the registers */
        rv32_init_class(core_class);

        /* define attributes for each register, from
           the contents of the register table */

        cosim_core_register_register_table_attributes(core_class);
}


/* functions that interface between a call by the cosim
   and the actual function in the processor */
int
cosim_core_logical_to_physical(cosim_core_t *core,
                                logical_address_t la_addr,
                                access_t access_type,
                                physical_address_t *pa_addr)
{
        /* no address translation: logical == physical */
        *pa_addr = la_addr;
        int rc = 1;
        if (!rc) {
                SIM_LOG_ERROR(core->obj, 0,
                              "logical address 0x%llx to physical address"
                              " failed", la_addr);
        } else {
                SIM_LOG_INFO(2, core->obj, 0,
                             "logical address 0x%llx to physical address"
                             " 0x%llx", la_addr, *pa_addr);
        }
        return rc;
}


/* get the current program counter for the core */
logical_address_t
cosim_core_get_pc(cosim_core_t *core)
{
        return rv32_get_pc(core);
}

/* set the current program counter for the core */
void
cosim_core_set_pc(cosim_core_t *core, logical_address_t value)
{
        rv32_set_pc(core, value);
}

bool
cosim_core_fetch_and_execute_instruction(cosim_core_t *core)
{
        rv32_execute(core);
        return true;
}

/* functions that interface between a call by the processor
   and the actual function in the cosim */
static cycles_t
queue_next_event(event_queue_t *queue)
{
        if (queue_is_empty(queue))
                return 1000;
        else
                return get_delta(queue);
}

void
cosim_core_increment_cycles(cosim_core_t *core, int cycles)
{
        cosim_t *cosim = associated_queue(core);
        if (cycles > 0) {
                queue_decrement(&(cosim->cycle_queue), cycles);
                cosim->current_cycle += cycles;
        }
}

void
cosim_core_increment_steps(cosim_core_t *core, int steps)
{
        cosim_t *cosim = associated_queue(core);
        if (steps > 0) {
                queue_decrement(&(cosim->step_queue), steps);
                cosim->current_step += steps;
        }
}

int
cosim_core_next_cycle_event(cosim_core_t *core)
{
        cosim_t *cosim = associated_queue(core);
        int cycles = queue_next_event(&cosim->cycle_queue);
        return cycles;
}

int
cosim_core_next_step_event(cosim_core_t *core)
{
        cosim_t *cosim = associated_queue(core);
        int steps;
        steps = queue_next_event(&cosim->step_queue);
        return steps;
}


execute_state_t
cosim_core_state(cosim_core_t *core)
{
        cosim_t *cosim = associated_queue(core);
        return cosim->state;
}

void
cosim_core_check_virtual_breakpoints(cosim_core_t *core,
                                      access_t access,
                                      logical_address_t virt_start,
                                      generic_address_t len,
                                      uint8 *data)
{
        cosim_t *cosim = associated_memory(core);
        check_virtual_breakpoints(cosim, core, access, virt_start, len, data);
}


bool
cosim_core_fetch_instruction(cosim_core_t *core,
                              physical_address_t pa, physical_address_t len,
                              uint8 *data, int check_bp)
{
        cosim_t *cosim = associated_memory(core);
        return fetch_instruction(cosim, core, pa, len, data, check_bp);
}


bool
cosim_core_write_memory(cosim_core_t *core,
                         physical_address_t pa, physical_address_t len,
                         uint8 *data, int check_bp)
{
        cosim_t *cosim = associated_memory(core);
        return write_memory(cosim, core, pa, len, data, check_bp);
}

bool
cosim_core_read_memory(cosim_core_t *core,
                        physical_address_t pa, physical_address_t len,
                        uint8 *data, int check_bp)
{
        cosim_t *cosim = associated_memory(core);
        return read_memory(cosim, core, pa, len, data, check_bp);
}
