# This Software is part of Simics. The rights to copy, distribute,
# modify, or otherwise make use of this Software may be licensed only
# pursuant to the terms of an applicable license agreement.
# 
# Copyright 2010-2021 Intel Corporation

import cli
import pci_common

cli.new_info_command("pci_data_capture", pci_common.get_pci_info)
cli.new_status_command("pci_data_capture", pci_common.get_pci_status)
pci_common.new_pci_config_regs_command('pci_data_capture', None)
