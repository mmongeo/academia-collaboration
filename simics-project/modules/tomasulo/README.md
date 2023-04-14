This file contains the tomasulo module for the project

Steps for load the x86 file into Simics:
1. Execute the following command 
    nasm example.asm -o example.bin
This will create a .bin file
2. Start the Simics simulation
3. Load the .bin file with the following command
    load-file filename = example.bin offset = 0x0fffffff0
This will load and place the intructions at the offset