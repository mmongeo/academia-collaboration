from constants import Units, MEM_DATA
from dev_util import (Memory, value_to_tuple_le, tuple_to_value_le)


class MainMemory:

    def __init__(self):
        self.blocks = 0x20
        self.block_size = 8 * Units.Byte
        self.size = self.blocks * self.block_size
        self.memory = Memory()

        i = 0
        for val in MEM_DATA:
            self.write(i*self.block_size, val, self.block_size)
            i+=1

    def write(self, addr, value, byte_len):
        if addr < self.size:
            self.memory.write(addr, value_to_tuple_le(value, byte_len))
        else: print("Memory address out of bounds")

    def read(self, addr, byte_len):
        return tuple_to_value_le(self.memory.read(addr, byte_len))

class Cache(MainMemory):

    # 10 Byte Block format 0x00 | 0x00000000 -> # 2B mem_address | 8B data
    def __init__(self):
        self.blocks = 5
        self.block_size = 10 * Units.Byte
        self.size = self.blocks * self.block_size
        self.memory = Memory()

class CacheController:

    def __init__(self):
        self.cache = Cache()
        self.memory = MainMemory()

    #########################################################################
    #                           Here goes your code
    # _______________________________________________________________________
        

controller = CacheController()
print(controller.memory.read(0x0, 8 * Units.Byte))