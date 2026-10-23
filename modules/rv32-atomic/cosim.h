/*
  cosim.h - the cosimulator: clock, step and event queues

  © 2026 Intel Corporation

  This software and the related documents are Intel copyrighted materials, and
  your use of them is governed by the express license under which they were
  provided to you ("License"). Unless the License provides otherwise, you may
  not use, modify, copy, publish, distribute, disclose or transmit this software
  or the related documents without Intel's prior written permission.

  This software and the related documents are provided as is, with no express or
  implied warranties, other than those that are expressly stated in the License.
*/

#ifndef COSIM_H
#define COSIM_H

#include <simics/base/types.h>
#include <simics/base/log.h>
#include <simics/base/bigtime.h>
#include <simics/processor/types.h>
#include <simics/model-iface/execute.h>
#include <simics/devs/frequency.h>

#include "cosim-local.h"

typedef struct cosim_core cosim_core_t;

#include "event-queue-types.h"
#include "cosim-core.h"
#include "cosim-exec.h"

typedef struct {
        conf_object_t *object;
        const frequency_listener_interface_t *iface;
        char *port_name;
} frequency_target_t;

typedef VECT(frequency_target_t) frequency_target_list_t;

typedef struct {
        conf_object_t *mem_space;
        physical_address_t paddr;
        physical_address_t size;
        access_t access;
        uint8 *data;
} cosim_page_t;

/*
 * cosim class
 */
typedef struct cosim {
        /* pointer to the corresponding Simics object */
        conf_object_t *obj;

        /* Pointer to cell */
        conf_object_t *cell;
        const cell_inspection_interface_t *cell_iface;

        /* currently selected core */
        cosim_core_t *current_core;
        conf_object_t *current_core_obj;

        /* page cache */
        VECT(cosim_page_t) cached_pages;

        /* execute state */
        execute_state_t state;

        /* cycle queue */
        cycles_t current_cycle;
        event_queue_t cycle_queue;

        /* step queue */
        pc_step_t current_step;
        event_queue_t step_queue;

        cycles_t stall_cycles;
        cycles_t total_stall_cycles;

        /* the offset that has to be added to the frequency-rescaled
           cycle counter to produce the absolute time */
        bigtime_t time_offset;

        /* frequency related data */
        uint64 freq_hz;
        conf_object_t *frequency_dispatcher;
        char *frequency_dispatcher_port;
        const simple_dispatcher_interface_t *frequency_dispatcher_iface;
        frequency_target_list_t frequency_targets;
} cosim_t;

static inline conf_object_t *
cosim_to_conf_obj(cosim_t *cosim)
{
        return cosim->obj;
}

static inline cosim_t *
conf_obj_to_cosim(conf_object_t *obj)
{
        return (cosim_t *) SIM_object_data(obj);
}

void cosim_cycle_event_posted(cosim_t *cosim);
void cosim_step_event_posted(cosim_t *cosim);

#endif
