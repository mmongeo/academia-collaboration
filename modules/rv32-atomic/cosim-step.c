/*
  cosim-step.c - the step queue

  © 2026 Intel Corporation

  This software and the related documents are Intel copyrighted materials, and
  your use of them is governed by the express license under which they were
  provided to you ("License"). Unless the License provides otherwise, you may
  not use, modify, copy, publish, distribute, disclose or transmit this software
  or the related documents without Intel's prior written permission.

  This software and the related documents are provided as is, with no express or
  implied warranties, other than those that are expressly stated in the License.
*/

#include "cosim-step.h"
#include "event-queue.h"
#include <simics/processor-api.h>
#include <simics/model-iface/step.h>

#ifndef COSIM_HEADER
 #define COSIM_HEADER "cosim.h"
#endif
#include COSIM_HEADER

static int
match_all(lang_void *data, lang_void *match_data)
{
        return 1;
}

/*
 * step queue interface functions
 */
static pc_step_t
get_step_count(conf_object_t *NOTNULL queue_obj)
{
        cosim_t *cosim = conf_obj_to_cosim(queue_obj);
        return cosim->current_step;
}

static void
post_step(conf_object_t *NOTNULL queue_obj,
          event_class_t *NOTNULL evclass,
          conf_object_t *NOTNULL poster_obj,
          pc_step_t steps,
          lang_void *user_data)
{
        cosim_t *cosim = conf_obj_to_cosim(queue_obj);
        if (steps < 0) {
                SIM_LOG_ERROR(cosim_to_conf_obj(cosim), 0,
                              "can not post on step < 0");
                return;
        }
        post_to_queue(&cosim->step_queue, steps, evclass, poster_obj, user_data);
        cosim_step_event_posted(cosim);
}

static void
cancel_step(conf_object_t *NOTNULL queue_obj,
            event_class_t *NOTNULL evclass,
            conf_object_t *NOTNULL poster_obj,
            int (*pred)(lang_void *data, lang_void *match_data),
            lang_void *match_data)
{
        cosim_t *cosim = conf_obj_to_cosim(queue_obj);
        remove_events(&cosim->step_queue, evclass, poster_obj,
                      pred == NULL ? match_all : pred, match_data);
}

static pc_step_t
find_next_step(conf_object_t *NOTNULL queue_obj,
               event_class_t *NOTNULL evclass,
               conf_object_t *NOTNULL poster_obj,
               int (*pred)(lang_void *data, lang_void *match_data),
               lang_void *match_data)
{
        cosim_t *cosim = conf_obj_to_cosim(queue_obj);
        return next_occurrence(&cosim->step_queue, evclass, poster_obj,
                               pred == NULL ? match_all : pred, match_data);
}

static attr_value_t
step_events(conf_object_t *NOTNULL queue_obj)
{
        cosim_t *cosim = conf_obj_to_cosim(queue_obj);

        VECT(attr_value_t) evs = VNULL;

        pc_step_t s = 0;
        event_t *e;
        FOR_EVENTS_IN_QUEUE(&cosim->step_queue, e) {
                char *desc =
                        e->evclass->describe
                        ? e->evclass->describe(e->obj, e->param)
                        : NULL;
                s += e->delta;
                attr_value_t a = SIM_make_attr_list(
                        4,
                        SIM_make_attr_object(e->obj),
                        SIM_make_attr_string(e->evclass->name),
                        SIM_make_attr_uint64(s),
                        SIM_make_attr_string(desc));
                VADD(evs, a);
                MM_FREE(desc);
        }
        attr_value_t ret = SIM_alloc_attr_list(VLEN(evs));
        VFORI(evs, i)
                SIM_attr_list_set_item(&ret, i, VGET(evs, i));
        VFREE(evs);
        return ret;
}

static pc_step_t
step_advance(conf_object_t *queue_obj, pc_step_t steps)
{
        /* do nothing */
        return steps;
}

/*
 * step_queue attribute functions
 */
static set_error_t
set_step_queue(conf_object_t *queue_obj, attr_value_t *val)
{
        cosim_t *cosim = conf_obj_to_cosim(queue_obj);
        return set_event_queue(&cosim->step_queue, val);
}

static attr_value_t
get_step_queue(conf_object_t *queue_obj)
{
        cosim_t *cosim = conf_obj_to_cosim(queue_obj);
        return events_to_attr_list(&cosim->step_queue, 0);
}

/*
 * steps attribute functions
 */
static set_error_t
set_steps(conf_object_t *queue_obj, attr_value_t *val)
{
        cosim_t *cosim = conf_obj_to_cosim(queue_obj);
        if (!SIM_attr_is_integer(*val) || SIM_attr_integer(*val) < 0) {
                return Sim_Set_Illegal_Value; /* should not happen */
        }
        cosim->current_step = SIM_attr_integer(*val);
        return Sim_Set_Ok;
}

static attr_value_t
get_steps(conf_object_t *queue_obj)
{
        const cosim_t *cosim = conf_obj_to_cosim(queue_obj);
        return SIM_make_attr_uint64(cosim->current_step);
}

void
instantiate_step_queue(cosim_t *cosim)
{
        init_event_queue(&cosim->step_queue, "step");
}

void
register_step_queue(conf_class_t *cls)
{
        static const step_interface_t step_iface = {
                .get_step_count = get_step_count,
                .post_step = post_step,
                .cancel_step = cancel_step,
                .find_next_step = find_next_step,
                .events = step_events,
                .advance = step_advance
        };
        SIM_REGISTER_INTERFACE(cls, step, &step_iface);

        SIM_register_attribute(
                cls, "steps",
                get_steps,
                set_steps,
                Sim_Attr_Optional | Sim_Init_Phase_Pre1,
                "i",
                "Time measured in steps from machine start.");

        SIM_register_attribute(
                cls, "step_queue",
                get_step_queue,
                set_step_queue,
                Sim_Attr_Optional | Sim_Init_Phase_Pre1,
                "[[osaii]*]",
                "((<i>object</i>, <i>evclass</i>, <i>value</i>, <i>slot</i>,"
                " <i>step</i>)*).");
}
