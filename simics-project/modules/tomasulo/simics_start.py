# © 2016 Intel Corporation
#
# This software and the related documents are Intel copyrighted materials, and
# your use of them is governed by the express license under which they were
# provided to you ("License"). Unless the License provides otherwise, you may
# not use, modify, copy, publish, distribute, disclose or transmit this software
# or the related documents without Intel's prior written permission.
#
# This software and the related documents are provided as is, with no express or
# implied warranties, other than those that are expressly stated in the License.


import instrumentation
#We can remove this or make a better function for some other purpose as an example @martin @aykull
def extra_status(obj):
    icount = 0
    for c in obj.connections:
        icount += c.icount
    return [("Instruction count",
             [("count", icount)])]
        
instrumentation.make_tool_commands(
    "tomasulo",
    object_prefix = "tomasulo",
    provider_requirements = "cpu_instrumentation_subscribe",
    provider_names = ("processor", "processors"),
    status_cmd_extend_fn = extra_status,
    new_cmd_doc = """
    Creates a new tomasulo engine that can be
    connected to <arg>processors</arg> which supports instrumentation.
    """)
