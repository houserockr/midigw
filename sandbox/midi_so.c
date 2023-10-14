#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <unistd.h>
#include <time.h>
#include "cmdline.h"

#define CLOCK_MODE CLOCK_MONOTONIC

static volatile int running = 1;
static volatile int trigger = 0;
static int serial_fd = -1;

#define MIDI_CC 0xB

//unsigned char midi_msg[] = {0xb0, 0x2c, 0x00};
unsigned char midi_msg[3];
static inline void sendCC()
{
    write(serial_fd, midi_msg, sizeof(midi_msg));
}

static void handler(int sig, siginfo_t* si, void* uc)
{
    if (sig == SIGRTMIN)
        trigger = 1;
    else if (sig == SIGINT)
        running = 0;
}

static void init_sig_handlers()
{
    struct sigaction sa;

    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = handler;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGRTMIN, &sa, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
    if (sigaction(SIGINT, &sa, NULL) == -1) {
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
    serial_fd = open(path, O_WRONLY);
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

int main(int argc, char** argv)
{
    // Parse comand line args ...
    struct gengetopt_args_info args;
    if (cmdline_parser (argc, argv, &args) != 0) {
        exit(EXIT_FAILURE);
    }

    midi_msg[0] = (MIDI_CC << 4) | (args.channel_arg);
    midi_msg[1] = args.message_arg;
    midi_msg[2] = args.value_arg;
    printf("MIDI message: %02x %02x %02x\n", midi_msg[0], midi_msg[1], midi_msg[2]);

    long ns_interval = bpm_to_ns(args.tempo_arg);
    printf("Tempo interval: %ld ns\n", ns_interval);

    // init ...
    init_serial(args.serial_arg);
    init_sig_handlers();
    init_timer(ns_interval);

    // loop
    while (running) {
        if (trigger) {
            trigger = 0;
            long t0 = get_now_ns();
            sendCC();
            long t1 = get_now_ns();
            printf("sendCC took %d\n", (t1-t0));
        }
    }

    close_serial();
    return 0;
}