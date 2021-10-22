# academia-collaboration
Contains Simics device model for basic data capture. 
A basic component and a basic target that loads with x86-qsp

Steps: 
1. pull from repo. 
2. cd to simics-project in the repo. 
3. call project setup or use ispm to use this folder.
4. make "device_name" from project workspace.

Driver steps:

PCI Driver Presentation link: [PCI-Driver](https://drive.google.com/file/d/130z67GF9dVOsVsGkzWpdofGcLA3bqxIQ/view?usp=sharing)

1. Download the craff image from here: [Craff link](https://drive.google.com/file/d/1Hrl3ZlBgfXd_BiBZwEvxHkptiOIPQ2gs/view?usp=sharing)
2. Upload the files of the driver using agent-manager to the OS inside Simics.
3. Compile with make and install the .ko file.

