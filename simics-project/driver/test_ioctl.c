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
#define READ_VALUE_FROM_PCI_DEVICE _IOR('c', 'c', int32_t *)
#define LOAD_PICTURE _IOR('d', 'd', int32_t *)

int main()
{
    int fd;
    int32_t value, number;
    const char *chr_dev_name = CHARACTER_DEVICE_DRIVER_PATH;

    printf("*********************************\n");
    printf(">>> Opening character device\n");
    fd = open(chr_dev_name, O_RDWR);
    if (fd < 0) {
        printf("Cannot open character device file...\n");
        return 0;
    }

    printf("Enter the value to send: ");
    scanf("%d",&number);
    printf("Writing value to character device\n");
    ioctl(fd, WR_VALUE, (int32_t*) &number); 

    printf("Reading value from character device\n");
    ioctl(fd, RD_VALUE, (int32_t*) &value);
    printf("Value is %d\n", value);

    printf("Enter the offset to read: ");
    scanf("%d",&number);
    printf("Writing offset to character device\n");
    ioctl(fd, WR_VALUE, (int32_t*) &number);
    ioctl(fd, READ_VALUE_FROM_PCI_DEVICE, (int32_t*) &value);
    printf("The device returned: %d\n", value);

    printf("Load picture? (yes = 1/no = 0): ");
    scanf("%d",&number);
    
    switch (number) {
    case 1:
        printf("Writing command to PCI-Express device\n");
        ioctl(fd, LOAD_PICTURE, 0);
        break;
    case 0:
        // do nothing
        break;
    default:
        break;
    }

    printf("Closing character device file\n");
    close(fd);
}
