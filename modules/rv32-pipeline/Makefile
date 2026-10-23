# -*- Makefile -*-

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

# Simics module makefile

# Class names. The processor class is the module name; the core class adds
# "-core". Keep both in sync with CMakeLists.txt and module_load.py.
MODULE_CLASSES = rv32-pipeline rv32-pipeline-core
MODULE_CFLAGS += -DCOSIM_CLASSNAME=rv32-pipeline
MODULE_CFLAGS += -DCOSIM_CORE_CLASSNAME=rv32-pipeline-core

# Your core
SRC_FILES += rv32-pipeline.c

# RV32I state and Simics glue
SRC_FILES += rv32-core.c

# Helper functions for your core
SRC_FILES += rv32-helpers.c

# Simics plumbing: the cosimulator (cosim.c and cosim-*.c: clock, events,
# memory) and its core object (cosim-core.c)
SRC_FILES += cosim.c cosim-core.c cosim-queue.c \
             cosim-frequency.c cosim-step.c cosim-cycle.c \
             cosim-memory.c cosim-exec.c \
             cosim-direct-memory.c event-queue.c

PYTHON_FILES = module_load.py txt_loader.py

EXTRA_MODULE_VPATH = cycle-common

MODULE_CFLAGS += -I$(SIMICS_REPO_ROOT)/core/src/include

SRC_FILES += generic-cycle-iface.c

SIMICS_API := latest
THREAD_SAFE:= yes

include $(MODULE_MAKEFILE)
