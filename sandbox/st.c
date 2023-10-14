//
// Access the Raspberry Pi System Timer registers directly. 
//
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdint.h>

#define PERIPHERAL_BASE 0x20000000   // For Pi 1 and 2
// #define PERIPHERAL_BASE 0x3F000000      // For Pi 3
#define SYSTEM_TIMER_OFFSET 0x3000
#define ST_BASE (PERIPHERAL_BASE + SYSTEM_TIMER_OFFSET) 
// #define ST_BASE 0x7E003000 // Pi Zero

// Sytem Timer Registers layout
typedef struct {
    uint32_t control_and_status;
    uint32_t counter_low;
    uint32_t counter_high;
    uint32_t compare_0;
    uint32_t compare_1;
    uint32_t compare_2;
    uint32_t compare_3;
} system_timer_t;

// Get access to the System Timer registers in user memory space.
system_timer_t * get_system_timer() 
{
    void *system_timer;
    int  fd;

    if ((fd = open("/dev/mem", O_RDWR | O_SYNC) ) < 0) {
        printf("can't open /dev/mem \n");
        exit(-1);
    }

    system_timer = mmap(
        NULL,
        4096,
        PROT_READ | PROT_WRITE,
        MAP_SHARED,
        fd,
        ST_BASE
    );

    close(fd);

    if (system_timer == MAP_FAILED) {
        printf("mmap error %d\n", (int)system_timer);  // errno also set!
        exit(-1);
    }
    return (system_timer_t*)system_timer;
}

int main(int argc, char **argv) 
{
    volatile system_timer_t* st = get_system_timer();
    const uint32_t delay = 526316;

    uint32_t next = st->counter_low + delay;
    while ( 1 ) {
        if (st->counter_low >= next) {
	    printf("Boom.\n");
	    next = st->counter_low + delay;
	}
    }
    return 0;
}


int main_old(int argc, char **argv) 
{
    volatile system_timer_t* system_timer = get_system_timer();
    int32_t t0, t1;

    while (1) {
        t0 = system_timer->counter_low;
        sleep(1);
        t1 = system_timer->counter_low;
        printf ("Elaspsed = %d\n", t1 - t0);
        printf ("Conter high = %d\n", system_timer->counter_high);
        t0 = t1;
    }
    return 0;
}

