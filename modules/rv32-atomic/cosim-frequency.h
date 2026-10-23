/*
  cosim-frequency.h - frequency

  © 2026 Intel Corporation

  This software and the related documents are Intel copyrighted materials, and
  your use of them is governed by the express license under which they were
  provided to you ("License"). Unless the License provides otherwise, you may
  not use, modify, copy, publish, distribute, disclose or transmit this software
  or the related documents without Intel's prior written permission.

  This software and the related documents are provided as is, with no express or
  implied warranties, other than those that are expressly stated in the License.
*/

#ifndef COSIM_FREQUENCY_H
#define COSIM_FREQUENCY_H

#include "cosim.h"

void register_frequency_interfaces(conf_class_t *cls);
void instantiate_frequency(cosim_t *cosim);
void finalize_frequency(cosim_t *cosim);

#endif
