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
#include "cmdline.h"

//#define CLOCK_MODE CLOCK_MONOTONIC
#define CLOCK_MODE CLOCK_REALTIME
//#define CLOCK_MODE CLOCK_MONOTONIC_RAW
//#define CLOCK_MODE CLOCK_PROCESS_CPUTIME_ID

static volatile int running = 1;
static volatile int trigger = 0;
static int serial_fd = -1;

#define MIDI_CC 0xB

//unsigned char midi_msg[] = {0xb0, 0x2c, 0x00};
static unsigned char midi_msg[3];
static struct timespec write_backoff = {0, 1};
static inline void sendCC()
{
    write(serial_fd, midi_msg, sizeof(midi_msg));
    //nanosleep(&write_backoff, NULL);
    //tcflush(serial_fd, TCOFLUSH);
    //fsync(serial_fd);
}

#ifdef DEBUG
static void print_siginfo(siginfo_t *si)
{
    timer_t *tidp;
    int or;

    tidp = si->si_value.sival_ptr;

    printf("\tsival_ptr = %p; ", si->si_value.sival_ptr);
    printf("\t*sival_ptr = 0x%lx\n", (long) *tidp);

    or = timer_getoverrun(*tidp);
    if (or == -1) {
        perror("timer_getoverrun");
        exit(EXIT_FAILURE);
    }
    else
        printf("\toverrun count = %d\n", or);
}
#endif

static void handler_timer(int sig, siginfo_t* si, void* uc)
{
    trigger = 1;        
}

static void handler_int(int sig, siginfo_t* si, void* uc)
{
    running = 0;
}

static void init_sig_handlers()
{
    struct sigaction sa_timer, sa_int;

    // handler for the timer
    sa_timer.sa_flags = SA_SIGINFO;
    sa_timer.sa_sigaction = handler_timer;
    sigemptyset(&sa_timer.sa_mask);
    if (sigaction(SIGRTMIN, &sa_timer, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    // handler for SIGINT
    sa_int.sa_flags = SA_SIGINFO;
    sa_int.sa_sigaction = handler_int;
    sigemptyset(&sa_int.sa_mask);
    if (sigaction(SIGINT, &sa_int, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
}

static void init_timer(long interval_ns)
{
    timer_t timerid;
    struct sigevent sev;
    struct itimerspec its;
    sigset_t mask;
    
    // Block timer signal temporarily
    sigemptyset(&mask);
    sigaddset(&mask, SIGRTMIN);
    if (sigprocmask(SIG_SETMASK, &mask, NULL) == -1) {
        perror("sigprocmask");
        exit(EXIT_FAILURE);
    }
   
    // Create the timer
    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = SIGRTMIN;
    sev.sigev_value.sival_ptr = &timerid;
    if (timer_create(CLOCK_MODE, &sev, &timerid) == -1) {
        perror("timer_create");
        exit(EXIT_FAILURE);
    }

    printf("timer ID is 0x%lx\n", (long) timerid);

    // Start the timer
    its.it_value.tv_sec = 0;
    its.it_value.tv_nsec = interval_ns;
    its.it_interval.tv_sec = 0;
    its.it_interval.tv_nsec = interval_ns;

    if (timer_settime(timerid, 0, &its, NULL) == -1) {
        perror("timer_settime");
        exit(EXIT_FAILURE);
    }

    // Unlock the timer signal, so that timer notification can be delivered
    if (sigprocmask(SIG_UNBLOCK, &mask, NULL) == -1) {
        perror("sigprocmask");
        exit(EXIT_FAILURE);
    }
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

static inline long bpm_to_ns(int bpm)
{
    if (bpm > 0)
        return (long)(60000000000 / bpm);
    return 0;
}

static inline long get_now_ns()
{
    static struct timespec now;
    clock_gettime(CLOCK_MODE, &now);
    return now.tv_nsec;
}

#ifdef DEBUG
#define dprint(fmt, ...) \
        do { fprintf(stderr, "%s:%d:%s(): " fmt, __FILE__, \
                                __LINE__, __func__, __VA_ARGS__); } while (0)
#else
#define dprint(fmt, ...)
#endif

int main(int argc, char** argv)
{
    // Parse comand line args ...
    struct gengetopt_args_info args;
    if (cmdline_parser (argc, argv, &args) != 0) {
        exit(EXIT_FAILURE);
    }

    // preprocess args
    // check serial port
    struct stat serial_stat;
    if (stat(args.serial_arg, &serial_stat) != 0) {
        perror("stat");
        exit(EXIT_FAILURE);
    }

    if (!S_ISCHR(serial_stat.st_mode)) {
        printf("%s is not a character device. Exiting.\n", args.serial_arg);
        exit(EXIT_FAILURE);
    }

    // check midi
    if (args.channel_arg < 0 || args.channel_arg > 15) {
        printf("MIDI channel needs to be in range 0-15. Exiting.\n", args.channel_arg);
        exit(EXIT_FAILURE);
    }

    midi_msg[0] = (MIDI_CC << 4) | (args.channel_arg);
    midi_msg[1] = args.message_arg;
    midi_msg[2] = args.value_arg;
    dprint("MIDI message: %02x %02x %02x\n", midi_msg[0], midi_msg[1], midi_msg[2]);

    long ns_interval = bpm_to_ns(args.tempo_arg);
    dprint("Tempo interval: %ld ns (%d bpm)\n", ns_interval, args.tempo_arg);

    // init ...
    init_serial(args.serial_arg);
    init_sig_handlers();
    init_timer(ns_interval);

    // loop
    int i = 0;
    while (running) {
        usleep(25000);
        if (trigger) {
            trigger = 0;
#ifdef DEBUG    
            long t0 = get_now_ns();
#endif
            sendCC();
#ifdef DEBUG
            long t1 = get_now_ns();
            dprint("sendCC took %d\n", (t1-t0));
#endif
            if (args.repeats_given && ++i >= args.repeats_arg) {
                break;
            }
        }
        
    }

    // clean up
    close_serial();
    return 0;
}

#ifdef OLDSTUFF
int main_nanosleep(int argc, char** argv)
{
    signal(SIGINT, int_handler);
    init_serial();
    
    const long delay = 526316000; // ns

    struct timespec req, rem;
    req.tv_sec = 0;
    req.tv_nsec = delay;
    long t0, t1;
    long diff;

    while (running) {
        t0 = getNow();
        sendCC();
        t1 = getNow();
        diff = t1 - t0;
        if (diff < 0)
            diff += 1000000000;
        printf("sendCC took: %ld\n", diff);

        t0 = getNow();
        nanosleep(&req, &rem);
        t1 = getNow();
        diff = t1 - t0;
        if (diff < 0)
            diff += 1000000000;
        printf("nanosleep took: %ld (%ld)\n", diff, (delay-diff));

    }
    

    close_serial();
    return 0;
}
#endif


#ifdef OLDSTUFF
int main(int argc, char** argv)
{
    signal(SIGINT, int_handler);
    init_serial();
    
    const long delay = 526316000; // ns
    long last = getNow();
    long diff = 0;
    long now  = 0;

    int i = 0;
    while (running) {
        now  = getNow();
        diff = now - last;
        if (diff < 0)
            diff += 1000000000;
        if (diff >= delay) {
            //printf("Boom\n");
            last = now;
            sendCC();

            if (++i % 8 == 0) {
                i = 0;
                printf("backing off ...\n");
                sleep(5);
            }

        }

        usleep(1);
    }

    close_serial();
    return 0;
}
#endif

#ifdef OLDSTUFF
int main(int argc, char** argv)
{
    signal(SIGINT, int_handler);
    init_serial();
    //init_timer();
    
    volatile core_timer_t* st = get_system_timer();
    const uint32_t delay = 526316;

    uint32_t next = st->counter_low + delay;
    while (running) {
        if (st->counter_low >= next) {
            printf("Boom\n");
            // sendCC();
            next = st->counter_low + delay;
        }
    }

    close_serial();
    return 0;
}
#endif

