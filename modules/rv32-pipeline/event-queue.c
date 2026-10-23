/*
  event-queue.c - event queue implementation

  © 2026 Intel Corporation

  This software and the related documents are Intel copyrighted materials, and
  your use of them is governed by the express license under which they were
  provided to you ("License"). Unless the License provides otherwise, you may
  not use, modify, copy, publish, distribute, disclose or transmit this software
  or the related documents without Intel's prior written permission.

  This software and the related documents are provided as is, with no express or
  implied warranties, other than those that are expressly stated in the License.
*/

#include "event-queue.h"

#include <simics/processor-api.h>
#include <simics/simulator-api.h>

#define DELTA(queue) ((queue)->events[(queue)->first].delta)

/*
   (re)allocate memory for events and set up free list,
   function assumes that the current queue is full or empty
*/
static void
alloc_queue(event_queue_t *queue, int newsize)
{
        ASSERT(queue->free_list == EVENT_QUEUE_END);
        if (queue->size >= newsize)
                return;
        queue->events = MM_REALLOC(queue->events, newsize, event_t);
        queue->free_list = queue->size;
        for (int i = queue->size; i < newsize - 1; i++)
                queue->events[i].next = i + 1;
        queue->events[newsize - 1].next = EVENT_QUEUE_END;
        queue->size = newsize;
}

void
init_event_queue(event_queue_t *queue, const char *name)
{
        queue->name = name;
        queue->first = EVENT_QUEUE_END;
        queue->size = 0;
        queue->events = NULL;
        queue->free_list = EVENT_QUEUE_END;
        alloc_queue(queue, 4);
}

static void
destroy_event(event_t *event)
{
        const event_class_t *ec = event->evclass;
        if (ec->destroy != NULL) {
                ec->destroy(event->obj, event->param);
        }
}

void
destroy_event_queue(event_queue_t *queue)
{
        event_t *event;
        for (int event_index = queue->first;
             event_index != EVENT_QUEUE_END;
             event_index = event->next) {
                event = &queue->events[event_index];
                destroy_event(event);
        }
        MM_FREE(queue->events);
}

static void
clear_event_queue(event_queue_t *queue)
{
        const char *name = queue->name;
        destroy_event_queue(queue);
        init_event_queue(queue, name);
}

static void
expand_queue(event_queue_t *queue)
{
        ASSERT(queue->free_list == EVENT_QUEUE_END);
        alloc_queue(queue, queue->size * 2);
        if (queue->size > SIM_attr_integer(SIM_get_attribute(
                                                   SIM_get_object("sim"),
                                                   "event_queue_max")))
                SIM_break_simulation("event queue overflow");
}

simtime_t
next_occurrence(const event_queue_t *queue, const event_class_t *evclass,
                const conf_object_t *obj,
                int (*pred)(lang_void *data, void *match_data),
                void *match_data)
{
        simtime_t delta = 0;
        event_t *event;
        for (int i = queue->first; i != EVENT_QUEUE_END; i = event->next) {
                event = &queue->events[i];
                delta += event->delta;
                if ((event->evclass == evclass)
                    && (event->obj == obj)
                    && pred(event->param, match_data)) {
                        return delta;
                }
        }
        return -1;
}



void
remove_events(event_queue_t *queue,
              event_class_t *evclass,
              conf_object_t *obj,
              int (*pred)(void *data, void *match_data),
              void *match_data)
{
        if (queue_is_empty(queue))
                return;

        event_t *event_prev = &queue->events[queue->first];
        int current = queue->first;
        int ctr = 0;

        while (current != EVENT_QUEUE_END) {
                ASSERT(ctr < queue->size);
                ctr += 1;
                event_t *event = &queue->events[current];
                if (event->evclass != evclass
                    || event->obj != obj
                    || !pred(event->param, match_data)) {
                        current = event->next;
                        event_prev = event;
                } else {
                        destroy_event(event);

                        if (event_prev == event) {
                                int new_first = event->next;
                                int new_free_list = current;
                                int new_current = event->next;

                                event->next = queue->free_list;

                                queue->first = new_first;
                                queue->free_list = new_free_list;
                                current = new_current;
                                if (current != EVENT_QUEUE_END)
                                        event_prev = &queue->events[current];
                        } else {
                                int new_prev_next = event->next;
                                int new_free_list = current;
                                int new_current = event->next;

                                event->next = queue->free_list;

                                event_prev->next = new_prev_next;
                                queue->free_list = new_free_list;
                                current = new_current;
                        }
                        if (current != EVENT_QUEUE_END) {
                                queue->events[current].delta +=
                                        event->delta;
                        }
                }
        }
}

void
post_to_queue(event_queue_t *queue,
              simtime_t when,
              event_class_t *evclass,
              conf_object_t *obj,
              lang_void *user_data)
{

        simtime_t delta = when;
        int *pe;
        int ctr = 0;
        for (pe = &queue->first; *pe != EVENT_QUEUE_END;
             pe = &queue->events[*pe].next) {
                ASSERT(ctr < queue->size);
                ctr += 1;
                event_t *event = &queue->events[*pe];
                if (delta < event->delta
                    || (delta == event->delta
                        && evclass->slot < event->slot)) {
                        break;  /* found place for new event */
                }
                delta -= event->delta;
        }

        /* insert new event before the one indexed *pe */
        int new_event = queue->free_list;
        if (unlikely(new_event == EVENT_QUEUE_END)) {
                expand_queue(queue);
                /* try again (pe may have moved). This should be a sibcall */
                post_to_queue(queue, when, evclass, obj, user_data);
        } else {
                event_t *ne = &queue->events[new_event];
                queue->free_list = ne->next;

                if (*pe != EVENT_QUEUE_END) {
                        queue->events[*pe].delta -= delta;
                }
                ne->next = *pe;
                *pe = new_event;

                ne->delta = delta;
                ne->slot = evclass->slot;
                ne->obj = obj;
                ne->evclass = evclass;
                ne->param = user_data;
        }
}

void
rescale_time_events(event_queue_t *queue, uint64 old_freq, uint64 new_freq)
{
        event_t *old_event_queue = queue->events;
        int next_old_event = queue->first;

        int size = queue->size;
        queue->first = EVENT_QUEUE_END;
        queue->size = 0;
        queue->events = NULL;
        queue->free_list = EVENT_QUEUE_END;
        alloc_queue(queue, size);

        simtime_t total_delta = 0;
        while (next_old_event != EVENT_QUEUE_END) {
                event_t *event = &old_event_queue[next_old_event];
                total_delta += event->delta;
                simtime_t new_time;
                new_time = (double)total_delta * new_freq / old_freq + 0.5;
                post_to_queue(queue, new_time, event->evclass,
                              event->obj, event->param);
                next_old_event = event->next;
        }
        MM_FREE(old_event_queue);
}

set_error_t
add_event_to_queue(event_queue_t *q, attr_value_t *ev)
{
        conf_object_t *obj;
        const char *ecname;
        attr_value_t *val;
        int64 slot;     /* ignored since it can be computed from the
                           event class */
        int64 when;
        bool ret = SIM_ascanf(ev, "osaii", &obj, &ecname, &val, &slot, &when);

        if (!ret || obj == NULL) {
                return Sim_Set_Illegal_Value;
        }
        event_class_t *evclass =
                SIM_get_event_class(SIM_object_class(obj), ecname);
        if (evclass == NULL) {
                strbuf_t s = sb_newf(
                        "Adding event for %s: %s has not been"
                        " registered as an event class in %s",
                        SIM_object_name(obj),
                        ecname,
                        SIM_get_class_name(SIM_object_class(obj)));
                SIM_attribute_error(sb_str(&s));
                sb_free(&s);
                return Sim_Set_Illegal_Value;
        }
        if (evclass->flags & Sim_EC_Notsaved) {
                strbuf_t s = sb_newf(
                        "%s for %s (%s) is not a saved event",
                        ecname, SIM_object_name(obj),
                        SIM_get_class_name(SIM_object_class(obj)));
                SIM_attribute_error(sb_str(&s));
                sb_free(&s);
                return Sim_Set_Illegal_Value;
        }
        void *param = evclass->set_value(obj, *val);
        post_to_queue(q, when, evclass, obj, param);
        return Sim_Set_Ok;
}

void
queue_decrement(event_queue_t *queue, simtime_t delta)
{
        if (queue->first != EVENT_QUEUE_END) {
                ASSERT(DELTA(queue) >= delta);
                DELTA(queue) -= delta;
        }
}

int
queue_is_empty(event_queue_t *queue)
{
        return queue->first == EVENT_QUEUE_END;
}

simtime_t
get_delta(event_queue_t *queue)
{
        simtime_t delta;

        if (queue->first == EVENT_QUEUE_END) {
                delta = -1;
        } else {
                delta = DELTA(queue);
        }
        return delta;
}

void
handle_next_event(event_queue_t *queue)
{
        ASSERT(queue->first != EVENT_QUEUE_END);

        event_t *event = &queue->events[queue->first];
        int tmp_e_next = event->next;
        conf_object_t *obj = event->obj;
        lang_void *param = event->param;
        void (*callback)(conf_object_t *, lang_void *)
                = event->evclass->callback;

        event->next = queue->free_list;
        queue->free_list = queue->first;
        queue->first = tmp_e_next;

        /* do callback last */
        callback(obj, param);
}

set_error_t
set_event_queue(event_queue_t *q, attr_value_t *val)
{
        clear_event_queue(q);
        for (unsigned i = 0; i < SIM_attr_list_size(*val); i++) {
                attr_value_t elem = SIM_attr_list_item(*val, i);
                set_error_t r = add_event_to_queue(q, &elem);
                if (r != Sim_Set_Ok)
                        return r;
        }
        return Sim_Set_Ok;
}

static attr_value_t
event_to_attr_val(event_t *event, simtime_t time)
{
        event_class_t *ec = event->evclass;
        if (ec->flags & Sim_EC_Notsaved)
                return SIM_make_attr_invalid(); /* don't save this attribute */

        if (!ec->get_value)
                return SIM_make_attr_invalid();

        attr_value_t val = ec->get_value(event->obj, event->param);
        if (SIM_attr_is_invalid(val))
                return SIM_make_attr_invalid(); /* don't save (for
                                                    nonregistered events) */
        return SIM_make_attr_list(5,
                                  SIM_make_attr_object(event->obj),
                                  SIM_make_attr_string(ec->name),
                                  val,
                                  SIM_make_attr_uint64(event->slot),
                                  SIM_make_attr_uint64(time));
}

attr_value_t
events_to_attr_list(event_queue_t *q, simtime_t start)
{
        attr_value_t ret = SIM_alloc_attr_list(q->size);
        event_t *event;
        simtime_t t = start;
        unsigned idx = 0;
        FOR_EVENTS_IN_QUEUE(q, event) {
                t += event->delta;
                attr_value_t a = event_to_attr_val(event, t);
                if (!SIM_attr_is_invalid(a))
                        SIM_attr_list_set_item(&ret, idx++, a);
        }

        SIM_attr_list_resize(&ret, idx);
        return ret;
}
