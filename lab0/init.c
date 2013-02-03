#include <stdio.h>
#include <unistd.h>
#include <sys/reboot.h>

int main(int argc, char **argv)
{
    /* This doesn't do anything, be sure to log that. */
    printf("[cs194-24] init running\n");

    /* Make sure that QEMU doesn't quit too quickly */
    sleep(10);

    /* Turn off the system -- this magic constant powers off */
    sync();
    reboot(0x4321fedc);
    
    return 0;
}
