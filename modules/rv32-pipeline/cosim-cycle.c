/*
  cosim-cycle.c - code to support the cycle queue

  © 2026 Intel Corporation

  This software and the related documents are Intel copyrighted materials, and
  your use of them is governed by the express license under which they were
  provided to you ("License"). Unless the License provides otherwise, you may
  not use, modify, copy, publish, distribute, disclose or transmit this software
  or the related documents without Intel's prior written permission.

  This software and the related documents are provided as is, with no express or
  implied warranties, other than those that are expressly stated in the License.
*/

#include "cosim-cycle.h"
#include "event-queue.h"
#include <simics/processor-api.h>

#include <simics/base/local-time.h>

#include "generic-cycle-iface.h"
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
 * Cycle queue interface
 */
static cycles_t
get_cycle_count(conf_object_t *queue_obj)
{
        cosim_t *cosim = conf_obj_to_cosim(queue_obj);
        return cosim->current_cycle;
}

static local_time_t
get_time_in_ps(conf_object_t *queue_obj)
{
        cosim_t *cosim = conf_obj_to_cosim(queue_obj);
        return generic_get_time_in_ps(queue_obj, cosim->time_offset,
                                      cosim->current_cycle, cosim->freq_hz);
}

static double
get_time(conf_object_t *queue_obj)
{
        return local_time_as_sec(get_time_in_ps(queue_obj));
}

static cycles_t
cycles_delta_from_ps(conf_object_t *queue_obj, local_time_t when)
{
        cosim_t *cosim = conf_obj_to_cosim(queue_obj);
        cycles_t delta;
        if (!generic_delta_from_ps(when, cosim->time_offset,
                                   cosim->current_cycle, cosim->freq_hz,
                                   &delta)) {
                char r[LOCAL_TIME_STR_MAX_SIZE];
                local_time_as_string(when, r);
                SIM_LOG_ERROR(queue_obj, 0, "cycles_delta_from_ps: time"
                              " delta to %s is too big to be"
                              " represented in cycles", r);
                return -1;
        } else {
                return delta;
        }
}

static cycles_t
cycles_delta(conf_object_t *queue_obj, double when)
{
        return cycles_delta_from_ps(queue_obj,
                                    local_time_from_sec(queue_obj, when));
}

static uint64
get_frequency(conf_object_t *queue_obj)
{
        cosim_t *cosim = conf_obj_to_cosim(queue_obj);
        return cosim->freq_hz;
}

static void
post_cycle(conf_object_t *NOTNULL queue_obj,
           event_class_t *NOTNULL evclass,
           conf_object_t *NOTNULL poster_obj,
           cycles_t cycles,
           lang_void *user_data)
{
        cosim_t *cosim = conf_obj_to_cosim(queue_obj);
        const char *err = check_post_cycle_params(cycles, evclass);
        if (err) {
                SIM_LOG_ERROR(queue_obj, 0, "%s", err);
                return;
        }
        post_to_queue(&cosim->cycle_queue, cycles, evclass, poster_obj,
                      user_data);
        cosim_cycle_event_posted(cosim);
}

static void
post_time(conf_object_t *NOTNULL queue_obj,
          event_class_t *NOTNULL evclass,
          conf_object_t *NOTNULL poster_obj,
          double seconds,
          lang_void *user_data)
{
        cosim_t *cosim = conf_obj_to_cosim(queue_obj);
        const char *err = generic_post_time(queue_obj, evclass, poster_obj,
                                            seconds, user_data,
                                            &post_cycle, cosim->freq_hz);
        if (err)
                SIM_LOG_ERROR(queue_obj, 0, "%s", err);
        cosim_cycle_event_posted(cosim);
}

static void
post_time_in_ps(conf_object_t *NOTNULL queue_obj,
                event_class_t *NOTNULL evclass,
                conf_object_t *NOTNULL poster_obj,
                duration_t picoseconds,
                lang_void *user_data)
{
        cosim_t *cosim = conf_obj_to_cosim(queue_obj);
        const char *err = generic_post_time_in_ps(queue_obj, evclass,
                                                  poster_obj,
                                                  picoseconds, user_data,
                                                  &post_cycle, cosim->freq_hz);
        if (err)
                SIM_LOG_ERROR(queue_obj, 0, "%s", err);
        cosim_cycle_event_posted(cosim);
}

static void
cancel(conf_object_t *NOTNULL queue_obj,
       event_class_t *NOTNULL evclass,
       conf_object_t *NOTNULL poster_obj,
       int (*pred)(lang_void *data, lang_void *match_data),
       lang_void *match_data)
{
        cosim_t *cosim = conf_obj_to_cosim(queue_obj);
        remove_events(&cosim->cycle_queue, evclass, poster_obj,
                      pred == NULL ? match_all : pred, match_data);
}

static cycles_t
find_next_cycle(conf_object_t *NOTNULL queue_obj,
                event_class_t *NOTNULL evclass,
                conf_object_t *NOTNULL poster_obj,
                int (*pred)(lang_void *data, lang_void *match_data),
                lang_void *match_data)
{
        cosim_t *cosim = conf_obj_to_cosim(queue_obj);
        cycles_t ret;
        ret = next_occurrence(&cosim->cycle_queue, evclass, poster_obj,
                               pred == NULL ? match_all :pred, match_data);
        return ret;
}

static double
find_next_time(conf_object_t *NOTNULL queue,
               event_class_t *NOTNULL evclass,
               conf_object_t *NOTNULL obj,
               int (*pred)(lang_void *data, lang_void *match_data),
               lang_void *match_data)
{
        const cosim_t *cosim = conf_obj_to_cosim(queue);
        return generic_find_next_time(queue, evclass, obj, pred,
                                      match_data, &find_next_cycle,
                                      cosim->freq_hz);
}

static duration_t
find_next_time_in_ps(conf_object_t *NOTNULL queue,
                     event_class_t *NOTNULL evclass,
                     conf_object_t *NOTNULL obj,
                     int (*pred)(lang_void *data,
                                 lang_void *match_data),
                     lang_void *match_data)
{
        const cosim_t *cosim = conf_obj_to_cosim(queue);
        return generic_find_next_time_in_ps(queue, evclass, obj, pred,
                                            match_data, &find_next_cycle,
                                            cosim->freq_hz);
}

static attr_value_t
cycle_events(conf_object_t *NOTNULL queue_obj)
{
        cosim_t *cosim = conf_obj_to_cosim(queue_obj);

        VECT(attr_value_t) evs = VNULL;

        cycles_t t = 0;
        event_t *e;
        FOR_EVENTS_IN_QUEUE(&cosim->cycle_queue, e) {
                char *desc =
                        e->evclass->describe
                        ? e->evclass->describe(e->obj, e->param)
                        : NULL;
                t += e->delta;
                attr_value_t a = SIM_make_attr_list(
                        4,
                        SIM_make_attr_object(e->obj),
                        SIM_make_attr_string(e->evclass->name),
                        SIM_make_attr_uint64(t),
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

/*
 * cycle_queue attributes functions
 */
static set_error_t
set_cycle_queue(conf_object_t *queue_obj, attr_value_t *val)
{
        cosim_t *cosim = conf_obj_to_cosim(queue_obj);
        return set_event_queue(&cosim->cycle_queue, val);
}

static attr_value_t
get_cycle_queue(conf_object_t *queue_obj)
{
        cosim_t *cosim = conf_obj_to_cosim(queue_obj);
        return events_to_attr_list(&cosim->cycle_queue, 0);
}

/*
 * cycles attributes functions
 */
static set_error_t
set_cycles(conf_object_t *queue_obj, attr_value_t *val)
{
        if (!SIM_attr_is_integer(*val) || SIM_attr_integer(*val) < 0) {
                return Sim_Set_Illegal_Value; /* should not happen */
        }
        cosim_t *cosim = conf_obj_to_cosim(queue_obj);
        cosim->current_cycle = SIM_attr_integer(*val);
        return Sim_Set_Ok;
}

static attr_value_t
get_cycles(conf_object_t *queue_obj)
{
        const cosim_t *cosim = conf_obj_to_cosim(queue_obj);
        return SIM_make_attr_uint64(cosim->current_cycle);
}

void
instantiate_cycle_queue(cosim_t *cosim)
{
        init_event_queue(&cosim->cycle_queue, "cycle");
}

void
register_cycle_queue(conf_class_t *cls)
{
        static const cycle_interface_t cycle_iface = {
                .get_cycle_count = get_cycle_count,
                .get_time = get_time,
                .cycles_delta = cycles_delta,
                .get_frequency = get_frequency,
                .post_cycle = post_cycle,
                .post_time = post_time,
                .cancel = cancel,
                .find_next_cycle = find_next_cycle,
                .find_next_time = find_next_time,
                .events = cycle_events,
                .get_time_in_ps = get_time_in_ps,
                .cycles_delta_from_ps = cycles_delta_from_ps,
                .post_time_in_ps = post_time_in_ps,
                .find_next_time_in_ps = find_next_time_in_ps
        };
        SIM_register_attribute(
                cls, "cycles",
                get_cycles,
                set_cycles,
                Sim_Attr_Optional | Sim_Init_Phase_Pre1,
                "i",
                "Time measured in cycles from machine start.");

        SIM_register_attribute(
                cls, "time_queue",
                get_cycle_queue,
                set_cycle_queue,
                Sim_Attr_Optional | Sim_Init_Phase_Pre1,
                "[[osaii]*]",
                "((<i>object</i>, <i>evclass</i>, <i>value</i>, <i>slot</i>,"
                " <i>step</i>)*).");

        SIM_register_clock(cls, &cycle_iface);
}
