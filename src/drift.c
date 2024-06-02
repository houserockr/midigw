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

#define CLOCK_MODE CLOCK_MONOTONIC
//#define CLOCK_MODE CLOCK_REALTIME
//#define CLOCK_MODE CLOCK_MONOTONIC_RAW
//#define CLOCK_MODE CLOCK_PROCESS_CPUTIME_ID

static volatile int running = 1;
static volatile int trigger = 0;

#define dprint(fmt, ...) \
        do { fprintf(stderr, "%s:%d:%s(): " fmt, __FILE__, \
                                __LINE__, __func__, __VA_ARGS__); } while (0)

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
    int r = clock_nanosleep(CLOCK_MODE, 0, &deadline, NULL);
    if (r != 0) {
        fprintf(stderr, "nanosleep: %d\n", r);
    }
}

static const long sleep_thr = 100000; // ns

void sleep_ns_p(long ns)
{
    struct timespec deadline;
    deadline.tv_sec = 0;

    long sleep_rem = ns;
    do {
        //deadline.tv_nsec = sleep_rem > sleep_thr ? sleep_thr : sleep_rem;
        deadline.tv_nsec = sleep_thr;
        clock_nanosleep(CLOCK_MODE, 0, &deadline, NULL);
        sleep_rem -= sleep_thr;
    } while (running && sleep_rem > 0);
}

static inline long bpm_to_ns(int bpm)
{
    if (bpm > 0)
        return (long)(60000000000 / bpm);
    return 0;
}

static timer_t timer;

static long timer_last = 0;
static long timer_waited = 0;
static long timer_drift = 0;
static long timer_interval_ns = 0;

static void handler_timer(int sig, siginfo_t* si, void* uc)
{
    long now = get_now_ns();
    timer_waited = now - timer_last;
    timer_last = now;
    if (timer_waited < 0)
        timer_waited += 1000000000;
    timer_drift = timer_interval_ns - timer_waited;
    
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
    sev.sigev_value.sival_ptr = &timer;
    if (timer_create(CLOCK_MODE, &sev, &timer) == -1) {
        perror("timer_create");
        exit(EXIT_FAILURE);
    }

    printf("timer ID is 0x%lx\n", (long) timer);

    // Start the timer
    its.it_value.tv_sec = 0;
    its.it_value.tv_nsec = interval_ns;
    its.it_interval.tv_sec = 0;
    its.it_interval.tv_nsec = interval_ns;

    if (timer_settime(timer, 0, &its, NULL) == -1) {
        perror("timer_settime");
        exit(EXIT_FAILURE);
    }

    // mark this as timer start
    timer_last = get_now_ns();

    // Unlock the timer signal, so that timer notification can be delivered
    if (sigprocmask(SIG_UNBLOCK, &mask, NULL) == -1) {
        perror("sigprocmask");
        exit(EXIT_FAILURE);
    }

    
}

static inline void adjust_timer(long interval_ns)
{
    struct itimerspec its;
    its.it_value.tv_sec = 0;
    its.it_value.tv_nsec = interval_ns;
    its.it_interval.tv_sec = 0;
    its.it_interval.tv_nsec = 0;//interval_ns;
    
    if (timer_settime(timer, 0, &its, NULL) == -1) {
        perror("timer_settime");
        exit(EXIT_FAILURE);
    }
}

void loop_nanosleep(long ns)
{
    while (running) {
        long t0 = get_now_ns();
        sleep_ns(ns);
        long t1 = get_now_ns();

        long waited = t1 - t0;
        if (waited < 0)
            waited += 1000000000;
        long drift = ns - waited;
        printf("slept: %ld drift: %ld\n", waited, drift);
        fflush(stdout);
    }
}

void loop_usleep(long ns)
{
    while (running) {
        long t0 = get_now_ns();
        usleep((ns / 1000));
        long t1 = get_now_ns();

        long waited = t1 - t0;
        if (waited < 0)
            waited += 1000000000;
        long drift = ns - waited;
        printf("slept: %ld drift: %ld\n", waited, drift);
        fflush(stdout);
    }
}

int main_foo(int argc, char** argv)
{
    init_sig_handlers();
    long min = 99999999;
    long max = 0;
    int i = 0;
    while (running) {
        long t0 = get_now_ns();
        long t1 = get_now_ns();
        usleep(10);
        long d = t1 - t0;
        if (d > max)
            max = d;
        if (d < min)
            min = d;
        printf("min: %ld max: %ld\n", min, max);
        usleep(100);
    }
    return 0;
}

int main(int argc, char** argv)
{
    const int bpm = 114;
    long ns_interval = bpm_to_ns(bpm);
    timer_interval_ns = ns_interval;
    dprint("Tempo interval: %ld ns (%d bpm)\n", ns_interval, bpm);

    // init
    init_sig_handlers();
    //init_timer(ns_interval);

    //loop_nanosleep(ns_interval);
    loop_usleep(ns_interval);

    // loop
    //int i = 0;
    while (running) {
        
        if (trigger) {
            trigger = 0;
            //if (timer_drift > 0)
            //    sleep_ns(timer_drift);

            printf("waited: %ld drift: %ld\n", timer_waited, timer_drift);
        }
        usleep(25000);
    }

    // clean up
    return 0;
}

