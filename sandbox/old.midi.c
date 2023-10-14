#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdint.h>

#define PERIPHERAL_BASE 0x20000000   // For Pi 1, 2 and zero
// #define PERIPHERAL_BASE 0x3F000000      // For Pi 3
#define SYSTEM_TIMER_OFFSET 0x3000
#define ST_BASE (PERIPHERAL_BASE + SYSTEM_TIMER_OFFSET)

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

volatile int running = 1;
volatile int trigger = 0;
int serial_fd = 0;
struct itimerval timer;

const unsigned char msg[] = {0xb0, 0x2c, 0x00};
const unsigned char msg_cmd = 0xb0;
const unsigned char msg_num = 0x2c;
const unsigned char msg_val = 0x00;
static inline void sendCC()
{
    write(serial_fd, msg, sizeof(msg));
}

void timer_handler(int signum)
{
    //printf("Boom\n");
    trigger = 1;
    //sendCC();
}

void int_handler(int signum)
{
    running = 0;
}


void init_timer()
{
    struct sigaction sa;

    sa.sa_handler = &timer_handler;
    sa.sa_flags   = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGALRM, &sa, NULL);

    getitimer(ITIMER_REAL, &timer);
    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = 526316;

    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec= 526316;

    setitimer(ITIMER_REAL, &timer, NULL);
}

void start_timer()
{
    setitimer(ITIMER_REAL, &timer, NULL);
}

void init_serial()
{
    serial_fd = open("/dev/ttyS0", O_WRONLY);
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

    tty.c_cflag &= ~PARENB;  // no parity bit
    tty.c_cflag &= ~CSTOPB;  // stop
    tty.c_cflag |= CS8;      // 8 bits per byte
    tty.c_cflag &= ~CRTSCTS; // disable flow control
    tty.c_lflag &= ~ICANON;  // disable canonical mode
    tty.c_lflag &= ~ECHO;    // Disable echo
    tty.c_lflag &= ~ECHOE;   // Disable erasure
    tty.c_lflag &= ~ECHONL;  // Disable new-line echo
    tty.c_lflag &= ~ISIG;    // Disable interpretation of INTR, QUIT and SUSP
    tty.c_iflag &= ~(IXON | IXOFF | IXANY); // Turn off s/w flow ctrl
    tty.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL); // Disable any special handling of received bytes

    cfsetispeed(&tty, 31250);
    cfsetospeed(&tty, 31250);

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

void test() 
{
    volatile system_timer_t* st = get_system_timer();
    const uint32_t delay = 526316;

    for (int i=0; i<100000; i++) {
	uint32_t t0 = st->counter_low;
	usleep(delay);
	uint32_t t1 = st->counter_low;
	printf("%d\n", t1-t0);
    }
}


int main(int argc, char** argv)
{
    signal(SIGINT, int_handler);
    init_serial();
    // init_timer();
    
    volatile system_timer_t* st = get_system_timer();
    const uint32_t delay = 526316;

    uint32_t next = st->counter_low + delay;
    while (running) {
        if (st->counter_low >= next) {
            sendCC();
            next = st->counter_low + delay;
        }
    }

    close_serial();
    return 0;
}

