# esta variable contiene el nombre de su driver compilado
# para este caso saldria un pci_capture_driver.ko
BINARY     := pci_capture_module

# flags necesarios para compilar el driver 
KERNEL      := /lib/modules/$(shell uname -r)/build
ARCH        := x86
C_FLAGS     := -Wall
KMOD_DIR    := $(shell pwd)

# aqui ponen el nombre de su archivo .c
# por ejemplo: su archivo se llama driver-de-pedro.c, entonces ponen driver-de-pedro.o
# el resto de cosas no tendrian que modificarlas
OBJECTS := pci_capture_driver.o

# todo lo demas queda como esta, no necesita cambios
ccflags-y += $(C_FLAGS)

obj-m += $(BINARY).o

$(BINARY)-y := $(OBJECTS)

$(BINARY).ko:
		make -C $(KERNEL) M=$(KMOD_DIR) modules

clean:
		rm -f $(BINARY).ko
		rm -f *.o *.cmd *.dwo *.mod *.mod.c *.order *.symvers 
