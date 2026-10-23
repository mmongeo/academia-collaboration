/*
  cosim-queue.c - queue interface of the cosimulator

  © 2026 Intel Corporation

  This software and the related documents are Intel copyrighted materials, and
  your use of them is governed by the express license under which they were
  provided to you ("License"). Unless the License provides otherwise, you may
  not use, modify, copy, publish, distribute, disclose or transmit this software
  or the related documents without Intel's prior written permission.

  This software and the related documents are provided as is, with no express or
  implied warranties, other than those that are expressly stated in the License.
*/

#include "cosim-queue.h"
#include "event-queue.h"
#ifndef COSIM_HEADER
 #define COSIM_HEADER "cosim.h"
#endif
#include COSIM_HEADER

void
handle_events(cosim_t *cosim, event_queue_t *queue)
{
        while (!queue_is_empty(queue)
               && get_delta(queue) == 0
               && cosim->state != State_Stopped) {
                /* Check for pending asynchronous events before
                   handling CPU event */
                VT_check_async_events();
                handle_next_event(queue);
        }
}
