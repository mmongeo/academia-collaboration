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

# Creates the RV32I course machine and loads the program. Run from
# rv32.simics, which declares the parameters used here.

import importlib

import cli
import simics
from simics import pre_conf_object

env = cli.simenv
cpu_class = env.cpu_class
core_class = cpu_class + "-core"

cosim = pre_conf_object("cosim0", cpu_class)
cosim.queue = cosim
cosim.freq_mhz = env.freq_mhz

ctx = pre_conf_object("ctx0", "context")
ctx.queue = cosim

image = pre_conf_object("ram0_image", "image")
image.queue = cosim
image.size = env.ram_size

ram = pre_conf_object("ram0", "ram")
ram.image = image

mem = pre_conf_object("phys_mem0", "memory-space")
mem.queue = cosim
mem.map = [[0, ram, 0, 0, env.ram_size]]

core = pre_conf_object("core0", core_class)
core.queue = cosim
core.cosim = cosim
core.physical_memory_space = mem
core.current_context = ctx

cell = pre_conf_object("cell0", "cell")
cell.current_processor = core
cell.current_step_obj = cosim
cell.current_cycle_obj = cosim
cell.scheduled_object = cosim

cosim.cell = cell
cosim.current_core = core

simics.SIM_add_configuration([cosim, ctx, image, ram, mem, core, cell], None)

core0 = simics.SIM_get_object("core0")
path = env.program
if path is not None:
    if not path.lower().endswith(".txt"):
        raise simics.SimExc_General(
            "%s: the program must be an assembly file (.txt)" % path)
    # Assembled by load_program() in the txt_loader.py of the core's module
    loader = importlib.import_module(
        "simmod.%s.txt_loader" % cpu_class.replace("-", "_"))
    loader.load_program(core0, path, env.load_address)
core0.iface.processor_info_v2.set_program_counter(env.load_address)
