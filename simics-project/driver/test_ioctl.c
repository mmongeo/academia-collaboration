#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#define CHARACTER_DEVICE_DRIVER_PATH "/dev/pci_capture_chr_dev-0"
#define WR_VALUE _IOW('a','a',int32_t *)
#define RD_VALUE _IOR('b','b',int32_t *)

int main()
{
    int fd;
    const char *chr_dev_name = CHARACTER_DEVICE_DRIVER_PATH;

    printf("*********************************\n");
    printf(">>> Opening character device\n");
    fd = open(chr_dev_name, O_RDWR);
    if (fd < 0) {
        printf("Cannot open character device file...\n");
        return 0;
    }
    printf("*********************************\n");

    int32_t buffer;
    ioctl(fd, RD_VALUE, (int32_t*) &buffer);
    printf("Read test_register: 0x%x\n", buffer);

    int32_t value = 0xCAFECAFE;
    ioctl(fd, WR_VALUE, (int32_t *) &value);
    printf("Write test_register succesfully done\n");

    printf("*********************************\n");
    printf(" >>> Closing character device\n");
    printf("*********************************\n");
    close(fd);
}
