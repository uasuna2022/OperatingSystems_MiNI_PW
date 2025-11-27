#define _GNU_SOURCE
#include <signal.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <time.h>

#define ERR(source) \
    (kill(0, SIGKILL), perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

volatile sig_atomic_t number_of_sent_SIGUSR2 = 0;
volatile sig_atomic_t number_of_received_SIGUSR2 = 0;
volatile sig_atomic_t last_received_sygnal = 0;

void set_handler(void (*f)(int), int sigNo)
{
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = f;
    if (sigaction(sigNo, &act, NULL) == -1)
        ERR("sigaction");
    return;
}

void set_handler_advanced(void (*f)(int, siginfo_t*, void*), int sigNo)
{
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_sigaction = f;
    act.sa_flags = SA_SIGINFO | SA_RESTART;
    if (sigaction(sigNo, &act, NULL) == -1)
        ERR("sigaction");
    return;
}

void parent_SIGUSR1_handler(int sigNo) 
{
    last_received_sygnal = SIGUSR1;
}
void parent_SIGUSR2_handler(int sigNo) 
{ 
    number_of_received_SIGUSR2++;
    last_received_sygnal = SIGUSR2;
}
void parent_SIGCHLD_handler(int sigNo, siginfo_t* info, void* context)
{
    pid_t wpid;
    int status;

    while((wpid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0)
    {
        if (WIFEXITED(status) || WIFSIGNALED(status))
            last_received_sygnal = SIGCHLD;
        if (WIFSTOPPED(status))
            last_received_sygnal = SIGSTOP;
        if (WIFCONTINUED(status))
            last_received_sygnal = SIGCONT;
    }
}


void usage(char* file)
{
    printf("USAGE: %s t n\n", file);
    printf("\tt - time in mikroseconds\n\t1:n - frequency of SIGUSR2 sygnal\n");
    exit(EXIT_FAILURE);
}

void child_work(int time, int n)
{
    long nanoseconds = time * 1000;
    struct timespec t = {0, nanoseconds};
    int counter = 1;
    while (1)
    {
        nanosleep(&t, NULL);
        if (counter % n == 0)
        {
            if (kill(getppid(), SIGUSR2) == -1)
                ERR("kill");
            number_of_sent_SIGUSR2++;
            printf("[%d] Child sent SIGUSR2. Total sent SIGUSR2 sygnals: %d\n",
                 getpid(), number_of_sent_SIGUSR2);
        }
        else 
        {
            if (kill(getppid(), SIGUSR1) == -1)
                ERR("kill");
        }
        counter++;
    }
}

void parent_work()
{
    sigset_t mask, old_mask;
    sigemptyset(&mask);
    set_handler(parent_SIGUSR1_handler, SIGUSR1);
    set_handler(parent_SIGUSR2_handler, SIGUSR2);
    set_handler_advanced(parent_SIGCHLD_handler, SIGCHLD);
    sigaddset(&mask, SIGUSR1);
    sigaddset(&mask, SIGUSR2);
    sigaddset(&mask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &mask, &old_mask);
    while (last_received_sygnal != SIGCHLD)
    {
        printf("[%d] Parent is waiting for a signal... (%d)\n", getpid(),
             number_of_received_SIGUSR2);
        sigsuspend(&old_mask);
    }


    while (wait(NULL) > 0) {}

    printf("[%d] Parent is leaving. Total received SIGUSR2 sygnals: %d\n",
         getpid(), number_of_received_SIGUSR2);

    exit(EXIT_SUCCESS);
}

int main(int argc, char** argv)
{
    if (argc != 3)
        usage(argv[0]);
     
    int time = atoi(argv[1]);
    int n = atoi(argv[2]);
    if (time <= 0 || n <= 0)
        usage(argv[0]);

    pid_t child_pid = fork();

    if (child_pid < 0)
        ERR("fork");
    if (child_pid == 0)
    {
        printf("Hi, I'm a child! My PID is %d\n", getpid());
        child_work(time, n);
        exit(EXIT_SUCCESS);
    }
    if (child_pid > 0)
        parent_work();

    exit(EXIT_SUCCESS);
}

