/*
  cosim-memory.h - the page cache and memory interface

  © 2026 Intel Corporation

  This software and the related documents are Intel copyrighted materials, and
  your use of them is governed by the express license under which they were
  provided to you ("License"). Unless the License provides otherwise, you may
  not use, modify, copy, publish, distribute, disclose or transmit this software
  or the related documents without Intel's prior written permission.

  This software and the related documents are provided as is, with no express or
  implied warranties, other than those that are expressly stated in the License.
*/

#ifndef COSIM_MEMORY_H
#define COSIM_MEMORY_H

#ifndef COSIM_HEADER
#define COSIM_HEADER "cosim.h"
#endif
#include COSIM_HEADER

#include "cosim.h"

void register_memory_interfaces(conf_class_t *cls);
void register_memory_attributes(conf_class_t *cls);

void init_page_cache(cosim_t *cosim);

void check_virtual_breakpoints(cosim_t *cosim,
                               cosim_core_t *core,
                               access_t access,
                               logical_address_t virt_start,
                               generic_address_t len,
                               uint8 *data);

bool write_memory(cosim_t *cosim,
                  cosim_core_t *core,
                  physical_address_t phys_address,
                  physical_address_t len,
                  uint8 *data, bool check_bp);

bool read_memory(cosim_t *cosim,
                 cosim_core_t *core,
                 physical_address_t phys_address,
                 physical_address_t len,
                 uint8 *data, bool check_bp);

bool fetch_instruction(cosim_t *cosim,
                       cosim_core_t *core,
                       physical_address_t phys_address,
                       physical_address_t len,
                       uint8 *data,
                       bool check_bp);

cosim_page_t *
get_page(cosim_t *cosim, cosim_core_t *core,
         conf_object_t *phys_mem_obj,
         physical_address_t address, access_t access);

cosim_page_t *
add_to_page_cache(cosim_t *cosim, cosim_page_t p);

cosim_page_t *
search_page_cache(cosim_t *cosim, conf_object_t *phys_mem_obj,
                  physical_address_t address);

void
clear_page_cache(cosim_t *cosim);

void register_local_memory_interfaces(conf_class_t *cls);
set_error_t local_core_set_phys_memory(cosim_core_t *core,
                                       conf_object_t *oval);

#endif
