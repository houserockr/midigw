#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <unistd.h>
#include <time.h>


//#define CLOCK_MODE CLOCK_MONOTONIC_RAW
#define CLOCK_MODE CLOCK_MONOTONIC
//#define CLOCK_MODE CLOCK_REALTIME

#define REG_BASE     0x40000000
#define ST_PRESCALER 0x40000008
#define ST_LOW       0x4000001C
#define ST_HIGH      0x40000020

//#define BAUDR 31250
#define BAUDR 9600


void* sys_timer = NULL;

static int serial_fd = -1;

static inline uint32_t get_st_ticks()
{
    return ((uint32_t*)sys_timer)[7];
}

static inline uint32_t us_to_st(uint32_t us)
{
    return (uint32_t)(us * 19.2f);
}

static inline void sendCC()
{
    static unsigned char midi_msg[3] = {0xb0, 0x2c, 0x00};
    write(serial_fd, midi_msg, sizeof(midi_msg));
}

static void init_serial(char* path)
{
    serial_fd = open(path, O_RDWR | O_NOCTTY | O_NDELAY);
    if (serial_fd < 0) {
        printf("Error %i from open: %s\n", errno, strerror(errno));
    } else {
        printf("Serial port is open (%d)\n", serial_fd);
    }

    struct termios tty;

    // Read in existing settings
    if (tcgetattr(serial_fd, &tty) != 0) {
        printf("Error %i from tcgetattr: %s\n", errno, strerror(errno));
    }

    tty.c_oflag = 0;         // raw output

    tty.c_cflag &= ~PARENB;  // no parity bit
    tty.c_cflag &= ~CSIZE;   // disable char size mask
    tty.c_cflag &= ~CSTOPB;  // stop
    tty.c_cflag |= CS8;      // 8 bits per byte
    tty.c_cflag &= ~CRTSCTS; // disable flow control


    tty.c_lflag &= ~ICANON;  // disable canonical mode
    tty.c_lflag &= ~ECHO;    // Disable echo
    tty.c_lflag &= ~ECHOE;   // Disable erasure
    tty.c_lflag &= ~ECHONL;  // Disable new-line echo
    tty.c_lflag &= ~IEXTEN;  // Disable extended processing
    tty.c_lflag &= ~ISIG;    // Disable interpretation of INTR, QUIT and SUSP
    tty.c_iflag &= ~(IXON | IXOFF | IXANY); // Turn off s/w flow ctrl
    tty.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL); // Disable any special handling of received bytes

    cfsetispeed(&tty, BAUDR);
    cfsetospeed(&tty, BAUDR);

    // Save tty settings
    if (tcsetattr(serial_fd, TCSANOW, &tty) != 0) {
        printf("Error %i from tcsetattr: %s\n", errno, strerror(errno));
    }
}

void close_serial()
{
    printf("Closing serial port (%d) ... ", serial_fd);
    if (close(serial_fd) == 0) {
        printf("done.\n");
    } else {
        printf("failed.\n");
        printf("Error %i close(): %s\n", errno, strerror(errno));
    }
}

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
    clock_gettime(CLOCK_MODE, &now);
    return now.tv_nsec;
}

static inline void sleep_ns(long ns)
{
    static struct timespec deadline;
    deadline.tv_sec = 0;
    deadline.tv_nsec = ns;
    clock_nanosleep(CLOCK_MODE, 0, &deadline, NULL);
}



int main(int argc, char** argv)
{
    if (argc != 2) {
        printf("Usage: %s serial port. Exiting.\n", argv[0]);
        exit(2);
    }

    sys_timer = get_system_timer();
    uint32_t delay_us = 526316; // 114 bpm in us
    uint32_t delay_tt = us_to_st(delay_us);

    init_serial(argv[1]);

    uint32_t next = get_st_ticks() + delay_tt;

    while (1) {

        if (get_st_ticks() >= next) {
            next = get_st_ticks() + delay_tt;
            sendCC();
        }

        usleep(100);

    }


    return 0;
}
