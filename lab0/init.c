#include <stdio.h>
#include <unistd.h>
#include <sys/reboot.h>

int main(int argc, char **argv)
{
    /* This doesn't do anything, be sure to log that. */
    printf("[cs194-24] init running\n");

    /* Turn off the system -- this magic constant powers off */
    sync();
    reboot(0x4321fedc);
    
    return 0;
}
