/*
  cosim-exec.c - executing instructions

  © 2026 Intel Corporation

  This software and the related documents are Intel copyrighted materials, and
  your use of them is governed by the express license under which they were
  provided to you ("License"). Unless the License provides otherwise, you may
  not use, modify, copy, publish, distribute, disclose or transmit this software
  or the related documents without Intel's prior written permission.

  This software and the related documents are provided as is, with no express or
  implied warranties, other than those that are expressly stated in the License.
*/

#include "cosim-exec.h"
#include "event-queue.h"
#ifndef COSIM_HEADER
#define COSIM_HEADER "cosim.h"
#endif
#include COSIM_HEADER
#include "cosim-queue.h"
#include <simics/processor-api.h>

static void
check_event_queues(cosim_t *cosim)
{
        handle_events(cosim, &cosim->cycle_queue);
        if (cosim->current_core->is_enabled) {
                handle_events(cosim, &cosim->step_queue);
        }
}

static void
exec_run(conf_object_t *obj)
{
        cosim_t *cosim = conf_obj_to_cosim(obj);

        cosim->cell_iface->set_current_processor_obj(
                cosim->cell, cosim_core_to_conf_obj(cosim->current_core));
        cosim->cell_iface->set_current_step_obj(cosim->cell, obj);

        SIM_LOG_INFO(2, cosim_to_conf_obj(cosim), 0, "running");
        cosim->state = State_Running;

        /* handle all events on the current step and cycle */
        check_event_queues(cosim);

        while (cosim->state == State_Running) {
                if (cosim->current_core->is_enabled
                    && cosim->stall_cycles == 0) {
                        if (!cosim_core_fetch_and_execute_instruction(
                                    cosim->current_core))
                                return;
                } else {
                        simtime_t delta = get_delta(&cosim->cycle_queue);

                        if (cosim->stall_cycles != 0) {
                                /* Cap time advance at current stall time,
                                   since then we want to get back to issuing
                                   instructions. */
                                if (delta > cosim->stall_cycles)
                                        delta = cosim->stall_cycles;

                                /* Stall as much as we can up to delta. */
                                cosim->total_stall_cycles += delta;
                                cosim->stall_cycles -= delta;
                        }

                        if (delta > 0) {
                                cosim_core_increment_cycles(cosim->current_core,
                                                             delta);
                        }
                }
                if (cosim->state == State_Stopped) // breakpoint triggered
                        break;

                /* handle all events on the current step and cycle */
                check_event_queues(cosim);
        }
}

static void
exec_stop(conf_object_t *obj)
{
        cosim_t *cosim = conf_obj_to_cosim(obj);
        SIM_LOG_INFO(2, cosim_to_conf_obj(cosim), 0, "stop");
        cosim->state = State_Stopped;
        VT_stop_event_processing(obj);
}

void
register_execute_interface(conf_class_t *cls)
{
        static const execute_interface_t execute_iface = {
                .run = exec_run,
                .stop = exec_stop
        };
        SIM_REGISTER_INTERFACE(cls, execute, &execute_iface);
}
