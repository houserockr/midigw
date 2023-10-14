#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdint.h>

static struct timespec now;

#define CLOCK_MODE CLOCK_MONOTONIC_RAW

static inline long getNow()
{
    clock_gettime(CLOCK_MODE, &now);
    return now.tv_nsec;
}

#define REG_BASE     0x40000000
#define ST_PRESCALER 0x40000008
#define ST_LOW       0x4000001C
#define ST_HIGH      0x40000020

void* get_system_timer()
{
    void* system_timer;
    int   fd;

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
        REG_BASE
    );

    close(fd);

    if (system_timer == MAP_FAILED) {
        printf("mmap error %p\n", system_timer);  // errno also set!
        exit(-1);
    }
    return system_timer;
}

static inline long get_now_ns()
{
    static struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    return now.tv_nsec;
}

void peek()
{
    void* mem = get_system_timer();
    int32_t pre = ((int32_t*)mem)[2];
    printf("pre scaler pre: %08x\n", pre);
    ((int32_t*)mem)[2] = 0x8000000;
    pre = ((int32_t*)mem)[2];
    printf("pre scaler post: %08x\n", pre);

    struct timespec req, rem;
    req.tv_sec = 0;
    req.tv_nsec = 1;
   
    uint32_t lastv = ((uint32_t*)mem)[7];
    long     lastt = get_now_ns();
    while ( 1 ) {
        uint32_t checkv = ((uint32_t*)mem)[7];
        long     checkt = get_now_ns();
        if (checkv != lastv) {
            printf("Change values %u > %u\n", lastv, checkv);
            printf("Change times %ld > %ld\n", lastt, checkt);
            lastv = checkv;
            lastt = checkt;

        }
    }

    while (1) { 
        uint32_t t0_lo = ((uint32_t*)mem)[7];
        uint32_t t0_hi = ((uint32_t*)mem)[8];

        int slept = 0;//nanosleep(&req, &rem);

        uint32_t t1_lo = ((uint32_t*)mem)[7];
        uint32_t t1_hi = ((uint32_t*)mem)[8];
        printf("slept: %d\n", slept);
        printf("t0: %u %u\n", t0_hi, t0_lo);
        printf("t1: %u %u\n", t1_hi, t1_lo);
        printf("td: %u\n", (t1_lo - t0_lo));
    }


#ifdef foo
    while (1) {
        int32_t pre = ((int32_t*)mem)[2];
        uint32_t tim = ((uint32_t*)mem)[7];
        printf("pre: %08x\n", pre);
        printf("tim: %u\n", tim);
        printf("##################\n");
    }
#endif
}

int main(int argc, char** argv)
{
    peek();

    struct timespec res;
    clock_getres(CLOCK_REALTIME, &res);
    printf("res of CLOCK_REALTIME: %ld %ld\n", res.tv_sec, res.tv_nsec);
    clock_getres(CLOCK_MONOTONIC, &res);
    printf("res of CLOCK_MONOTONIC: %ld %ld\n", res.tv_sec, res.tv_nsec);

    long t0 = getNow();
    usleep(526316);
    long t1 = getNow();
    printf("t0 %ld\n", t0);
    printf("t1 %ld\n", t1);
    printf("dt %ld\n", (t1-t0));

    printf("\n");
    long t2 = getNow();
    long t3 = getNow();
    printf("t2 %ld\n", t2);
    printf("t3 %ld\n", t3);
    printf("dt %ld\n", (t3-t2));

    printf("\n");
    struct timespec t4, t5;
    clock_gettime(CLOCK_MODE, &t4);
    clock_gettime(CLOCK_MODE, &t5);
    printf("t4 %ld\n", t4.tv_nsec);
    printf("t5 %ld\n", t5.tv_nsec);
    printf("dt %ld\n", (t5.tv_nsec - t4.tv_nsec));

    const long delay = 526316000; // ns
    struct timespec now;

    clock_gettime(CLOCK_REALTIME, &now);
    long last = now.tv_nsec;
    long diff = 0;

    while ( 1 ) {
        
        clock_gettime(CLOCK_REALTIME, &now);
        diff = now.tv_nsec - last;
        if (diff < 0)
            diff += 1000000000; // 1ns
        if (diff >= delay) {
            printf("Boom\n");
            last = now.tv_nsec;
        }
    }

    return 0;
}
