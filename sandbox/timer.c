 #include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <time.h>

/* This could be CLOCK_MONOTONIC or CLOCK_REALTIME depending on what you want */
#define CLOCKID CLOCK_REALTIME

static void
print_siginfo(siginfo_t *si)
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

static void
handler(int sig, siginfo_t *si, void *uc)
{
   /* Note: calling printf() from a signal handler is not
      strictly correct, since printf() is not async-signal-safe;
      see signal(7) */
   printf("Caught signal %d\n", sig);
   print_siginfo(si);
   //signal(sig, SIG_IGN);
}

int
main(int argc, char *argv[])
{
   timer_t timerid;
   struct sigevent sev;
   struct itimerspec its;
   long long freq_nanosecs;
   sigset_t mask;
   struct sigaction sa;

   if (argc != 3) {
       fprintf(stderr, "Usage: %s <sleep-secs> <freq-nanosecs>\n",
               argv[0]);
       exit(EXIT_FAILURE);
   }

   /* Establish handler for timer signal */
   printf("Establishing handler for signal %d\n", SIGRTMIN);
   sa.sa_flags = SA_SIGINFO;
   sa.sa_sigaction = handler;
   sigemptyset(&sa.sa_mask);
   if (sigaction(SIGRTMIN, &sa, NULL) == -1) {
    perror("sigaction");
    exit(EXIT_FAILURE);
   }

   /* Block timer signal temporarily */
   printf("Blocking signal %d\n", SIGRTMIN);
   sigemptyset(&mask);
   sigaddset(&mask, SIGRTMIN);
   if (sigprocmask(SIG_SETMASK, &mask, NULL) == -1) {
    perror("sigprocmask");
    exit(EXIT_FAILURE);
   }
   
   /* Create the timer */
   sev.sigev_notify = SIGEV_SIGNAL;
   sev.sigev_signo = SIGRTMIN;
   sev.sigev_value.sival_ptr = &timerid;
   if (timer_create(CLOCKID, &sev, &timerid) == -1) {
    perror("timer_create");
    exit(EXIT_FAILURE);
   }

   printf("timer ID is 0x%lx\n", (long) timerid);

   /* Start the timer */
   freq_nanosecs = atoll(argv[2]);
   its.it_value.tv_sec = freq_nanosecs / 1000000000;
   its.it_value.tv_nsec = freq_nanosecs % 1000000000;
   its.it_interval.tv_sec = its.it_value.tv_sec;
   its.it_interval.tv_nsec = its.it_value.tv_nsec;

   if (timer_settime(timerid, 0, &its, NULL) == -1) {
    perror("timer_settime");
    exit(EXIT_FAILURE);
   }

   /* Sleep for a while; meanwhile, the timer may expire
      multiple times */
   printf("Sleeping for %d seconds\n", atoi(argv[1]));
   sleep(atoi(argv[1]));

   /* Unlock the timer signal, so that timer notification
      can be delivered */
   printf("Unblocking signal %d\n", SIGRTMIN);
   if (sigprocmask(SIG_UNBLOCK, &mask, NULL) == -1) {
        perror("sigprocmask");
        exit(EXIT_FAILURE);
   }

   while (1);

   exit(EXIT_SUCCESS);
}
