# © 2026 Intel Corporation
#
# This software and the related documents are Intel copyrighted materials, and
# your use of them is governed by the express license under which they were
# provided to you ("License"). Unless the License provides otherwise, you may
# not use, modify, copy, publish, distribute, disclose or transmit this software
# or the related documents without Intel's prior written permission.
#
# This software and the related documents are provided as is, with no express or
# implied warranties, other than those that are expressly stated in the License.

if(MSVC)
  return()
endif()

# Keep the class names in sync with the Makefile and module_load.py.
simics_add_module(rv32-pipeline
  CLASSES rv32-pipeline rv32-pipeline-core
  SOURCES rv32-pipeline.c rv32-core.c rv32-helpers.c
          cosim.c cosim-core.c cosim-queue.c
          cosim-frequency.c cosim-step.c cosim-cycle.c
          cosim-memory.c cosim-exec.c cosim-direct-memory.c
          event-queue.c module_load.py txt_loader.py
  SIMICS_API latest
  INIT_LOCAL
)
target_link_libraries(rv32-pipeline
  PRIVATE cycle-common  # cycle-common is a STATIC library in core
)
target_compile_definitions(rv32-pipeline PRIVATE
  COSIM_CLASSNAME=rv32-pipeline
  COSIM_CORE_CLASSNAME=rv32-pipeline-core)
