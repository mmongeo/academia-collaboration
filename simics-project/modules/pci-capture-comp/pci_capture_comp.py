# This Software is part of Simics. The rights to copy, distribute,
# modify, or otherwise make use of this Software may be licensed only
# pursuant to the terms of an applicable license agreement.
# 
# Copyright 2010-2021 Intel Corporation

# <add id="sample_components.py" label="sample_components.py">
# <insert-until text="# END sample_components.py"/>
# </add>
import simics
from comp import StandardComponent, SimpleConfigAttribute, Interface

class pci_capture_comp(StandardComponent):
    """Component that instantiates all the devices for the pci capture card"""
    _class_desc = "PCI capture component"
    _help_categories = ('PCI',)

    def setup(self):
        super().setup()
        if not self.instantiated.val:
            self.add_objects()
        self.add_connectors()

    def add_objects(self):
        sd = self.add_pre_obj('dev', 'pci_data_capture')

    def add_connectors(self):
        self.add_connector(slot = 'pci_bus', type = 'pci-bus',
                           hotpluggable = True, required = False, multi = False,
                           direction = simics.Sim_Connector_Direction_Up)

    class basename(StandardComponent.basename):
        """The default name for the created component"""
        val = "pci_capture"

    class component_connector(Interface):
        """Uses connector for handling connections between components."""
        def get_check_data(self, cnt):
            return []
        def get_connect_data(self, cnt):
            return [[[0, self._up.get_slot('dev')]]]
        def check(self, cnt, attr):
            return True
        def connect(self, cnt, attr):
            self._up.get_slot('dev').pci_bus = attr[1]
        def disconnect(self, cnt):
            self._up.get_slot('dev').pci_bus = None

# END sample_components.py

