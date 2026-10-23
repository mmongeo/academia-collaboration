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

import cli
import conf
import simics
import sim_commands

def get_cosim_info(obj):
    return []

def get_cosim_status(obj):
    return []

def get_cosim_core_info(obj):
    return []

def get_cosim_core_status(obj):
    return []

def register_info_status(cosim_classname, core_classname):
    cli.new_info_command(cosim_classname, get_cosim_info)
    cli.new_status_command(cosim_classname, get_cosim_status)
    cli.new_info_command(core_classname, get_cosim_core_info)
    cli.new_status_command(core_classname, get_cosim_core_status)

# Function called by the 'pregs' command. Print common registers if
# all is false, and print more registers if all is true.
def local_pregs(obj, all):
    ir = obj.iface.int_register
    lines = []
    for row in range(0, 32, 4):
        lines.append("   ".join(
            "x%-2d = 0x%08x" % (r, ir.read(ir.get_number("x%d" % r)))
            for r in range(row, row + 4)))
    lines.append("pc  = 0x%08x" % obj.iface.processor_info.get_program_counter())
    return "\n".join(lines)

# Function used to track register changes when using stepi -r.
def local_diff_regs(obj):
    return tuple("x%d" % r for r in range(1, 32))

# Function used by default disassembler to indicate that the next
# step in the system will be an exception/interrupt.
def local_pending_exception(obj):
    return None

def register_processor_cli(core_classname):
    processor_cli_iface = simics.processor_cli_interface_t()
    processor_cli_iface.get_disassembly = sim_commands.make_disassembly_fun()
    processor_cli_iface.get_pregs = local_pregs
    processor_cli_iface.get_diff_regs = local_diff_regs
    processor_cli_iface.get_pending_exception_string = local_pending_exception
    processor_cli_iface.get_address_prefix = None
    processor_cli_iface.translate_to_physical = None
    simics.SIM_register_interface(core_classname, 'processor_cli',
                                  processor_cli_iface)

def register_opcode_info(core_classname):
    opcode_info = simics.opcode_length_info_t(min_alignment = 4,
                                       max_length = 4,
                                       avg_length = 4)

    simics.SIM_register_interface(
        core_classname, 'opcode_info',
        simics.opcode_info_interface_t(get_opcode_length_info
                                       = lambda cpu: opcode_info))

def register_classes(cosim_classname, core_classname):
    register_info_status(cosim_classname, core_classname)
    register_processor_cli(core_classname)
    register_opcode_info(core_classname)

# Keep the class names in sync with the Makefile and CMakeLists.txt.
register_classes("rv32-pipeline", "rv32-pipeline-core")
