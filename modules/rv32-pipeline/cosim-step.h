/*
  cosim-step.h - the step queue

  © 2026 Intel Corporation

  This software and the related documents are Intel copyrighted materials, and
  your use of them is governed by the express license under which they were
  provided to you ("License"). Unless the License provides otherwise, you may
  not use, modify, copy, publish, distribute, disclose or transmit this software
  or the related documents without Intel's prior written permission.

  This software and the related documents are provided as is, with no express or
  implied warranties, other than those that are expressly stated in the License.
*/

#ifndef COSIM_STEP_H
#define COSIM_STEP_H

#ifndef COSIM_HEADER
#define COSIM_HEADER "cosim.h"
#endif
#include COSIM_HEADER

#if defined(__cplusplus)
extern "C" {
#endif

void instantiate_step_queue(cosim_t *cosim);
void register_step_queue(conf_class_t *cls);

#if defined(__cplusplus)
}
#endif

#endif
