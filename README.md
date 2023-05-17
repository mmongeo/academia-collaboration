# academia-collaboration
Contains Simics device model for basic data capture. 
A basic component and a basic target that loads with x86-qsp

*Tomasulo Project*
Requirements
 - Have Nasm 

Steps for load the x86 file into Simics:
1. Execute the following command 
    nasm example.asm -o example.bin
This will create a .bin file
2. Start the Simics simulation
3. Load the .bin file with the following command
    load-file filename = example.bin offset = 0x0
This will load and place the intructions at the offset
NOTE: Currently the 3rd step is automated in the tomasulo.simics script but
      it is necessary to compile the .asm.

*PCI Device Project*
Driver steps:
PCI Driver Presentation link: [PCI-Driver-Presentation](https://drive.google.com/file/d/1HJH5QXc6Vq-CjpyutNHL_0jMnbwRRAA3/view?usp=sharing)

1. Download the craff image from here: [Craff link](https://drive.google.com/file/d/1Hrl3ZlBgfXd_BiBZwEvxHkptiOIPQ2gs/view?usp=sharing)
2. Upload the files of the driver using agent-manager to the OS inside Simics.
3. Compile with make and install the .ko file.

*Build a device*
Steps: 
1. pull from repo. 
2. cd to simics-project in the repo. 
3. call project setup or use ispm to use this folder.
4. make "device_name" from project workspace.