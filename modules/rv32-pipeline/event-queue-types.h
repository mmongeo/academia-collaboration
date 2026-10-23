/*
  event-queue-types.h - event queue types

  © 2026 Intel Corporation

  This software and the related documents are Intel copyrighted materials, and
  your use of them is governed by the express license under which they were
  provided to you ("License"). Unless the License provides otherwise, you may
  not use, modify, copy, publish, distribute, disclose or transmit this software
  or the related documents without Intel's prior written permission.

  This software and the related documents are provided as is, with no express or
  implied warranties, other than those that are expressly stated in the License.
*/

#ifndef EVENT_QUEUE_TYPES_H
#define EVENT_QUEUE_TYPES_H

#include <simics/base/types.h>
#include <simics/base/event.h>
#include <simics/base/time.h>
#include <simics/processor/types.h>

#define EVENT_QUEUE_END (-1)

typedef struct {
        int next;               /* index of next event, or EVENT_QUEUE_END */
        simtime_t delta;        /* clocks until next event */
        int slot;               /* determines the order when the expire time
                                   is the same for multiple events */
        event_class_t *evclass; /* class event belongs to */
        conf_object_t *obj;     /* object event operates on */
        lang_void *param;       /* user data */
} event_t;

typedef struct {
        const char *name;       /* queue name */
        event_t *events;        /* event array */
        int size;               /* size of allocated event array */
        int first;              /* index of first event */
        int free_list;          /* index of first free slot */
} event_queue_t;

#endif
