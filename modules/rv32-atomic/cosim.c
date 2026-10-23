/*
  cosim.c - the cosimulator: clock, step and event queues

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
#include <simics/processor-api.h>

// TODO: should not be needed, violates the Processor API
#include <simics/simulator/hap-consumer.h>

#include "event-queue.h"
#include "cosim-memory.h"
#include "cosim-exec.h"
#include "cosim-cycle.h"
#include "cosim-step.h"
#include "cosim-core.h"
#include "cosim-frequency.h"

#define STRINGIFY(x) STRINGIFY1(x)
#define STRINGIFY1(x) #x

/*
 * current core attribute functions
 */
static set_error_t
set_current_core(conf_object_t *obj,
                 attr_value_t *val)
{
        cosim_t *cosim = conf_obj_to_cosim(obj);
        cosim->current_core_obj = SIM_attr_object(*val);
        cosim->current_core = SIM_object_data(cosim->current_core_obj);
        return Sim_Set_Ok;
}

static attr_value_t
get_current_core(conf_object_t *obj)
{
        cosim_t *cosim = conf_obj_to_cosim(obj);
        return SIM_make_attr_object(cosim->current_core_obj);
}

static void
cosim_cell_change(lang_void *callback_data, conf_object_t *obj,
               conf_object_t *old_cell, conf_object_t *new_cell)
{
        cosim_t *cosim = conf_obj_to_cosim(obj);
        if (cosim->cell) {
                const simple_dispatcher_interface_t *iface =
                        SIM_c_get_port_interface(cosim->cell,
                                                 SIMPLE_DISPATCHER_INTERFACE,
                                                 "breakpoint_change");
                ASSERT(iface);
                iface->unsubscribe(cosim->cell, obj, NULL);
        }

        cosim->cell = new_cell;
        if (cosim->cell) {
                cosim->cell_iface = SIM_get_interface(new_cell,
                                                   CELL_INSPECTION_INTERFACE);

                const simple_dispatcher_interface_t *iface =
                        SIM_c_get_port_interface(cosim->cell,
                                                 SIMPLE_DISPATCHER_INTERFACE,
                                                 "breakpoint_change");
                ASSERT(iface);
                iface->subscribe(cosim->cell, obj, NULL);
        } else {
                cosim->cell_iface = NULL;
        }
}

static void *
cosim_init_object(conf_object_t *obj)
{
        cosim_t *cosim = MM_ZALLOC(1, cosim_t);
        cosim->obj = obj;
        VINIT(cosim->cached_pages);
        instantiate_step_queue(cosim);
        instantiate_cycle_queue(cosim);
        instantiate_frequency(cosim);

        SIM_hap_add_callback_obj("Core_Conf_Clock_Change_Cell", cosim->obj, 0,
                                 (obj_hap_func_t)cosim_cell_change, NULL);
        VT_set_object_clock(obj, obj);
        return cosim;
}

static void
cosim_finalize(conf_object_t *obj)
{
        cosim_t *cosim = conf_obj_to_cosim(obj);
        finalize_frequency(cosim);
}

static conf_class_t *
cosim_define_class()
{
        return SIM_create_class(
                STRINGIFY(COSIM_CLASSNAME),
                &(class_info_t){
                  .init = cosim_init_object,
                  .finalize = cosim_finalize,
                  .short_desc = "RV32I course processor",
                  .description =
                         "Clock, step and event queues for the RV32I course"
                         " core."
                });
}

static void
cosim_register_attributes(conf_class_t *cosim_class)
{
        SIM_register_attribute(
                cosim_class, "current_core",
                get_current_core,
                set_current_core,
                Sim_Attr_Required,
                "o",
                "Currently active core used for commands that normally"
                " target the current processor.");
}

void
cosim_cycle_event_posted(cosim_t *cosim)
{
        /* An event has been posted. We only single step, so we don't need to
           bother. */
}

void
cosim_step_event_posted(cosim_t *cosim)
{
        /* An event has been posted. We only single step, so we don't need to
           bother. */
}

/*
 * Called once when the device module is loaded into Simics.
 */
void
init_local()
{
        conf_class_t *cosim_class = cosim_define_class();
        cosim_register_attributes(cosim_class);
        register_memory_interfaces(cosim_class);
        register_memory_attributes(cosim_class);
        register_execute_interface(cosim_class);
        register_cycle_queue(cosim_class);
        register_step_queue(cosim_class);
        register_frequency_interfaces(cosim_class);

        init_cosim_core_class();
}
