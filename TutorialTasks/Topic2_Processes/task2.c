#define _GNU_SOURCE
#include <signal.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define ERR(source) \
    (kill(0, SIGKILL), perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

void usage(char* path)
{
    printf("USAGE:%s n k p r\nn,r,k,p>=1\n", path);
    exit(EXIT_FAILURE);
}

void set_handler(void(*f)(int), int sigNo)
{
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = f;
    if (sigaction(sigNo, &act, NULL) == -1)
        ERR("sigaction");
    return;
}

volatile sig_atomic_t last_signal = 0;
void sigusr1_handler(int sig)
{
    last_signal = 1;
}
void sigusr2_handler(int sig)
{
    last_signal = 2;
}

void child_work(int r)
{
    srand(getpid());
    int sleep_seconds = 5 + rand() % 6;

    for (int i = 1; i <= r; i++)
    {
        printf("[%d] Sleeping for %d seconds...\n", getpid(), sleep_seconds);
        sleep(sleep_seconds);
        if (last_signal == 0)
            printf("[%d] Got no sygnal yet...\n", getpid());
        if (last_signal == 1)
            printf("[%d] Success!\n", getpid());
        if (last_signal == 2)
            printf("[%d] Failure!\n", getpid());
    }
}

void parent_work(int n, int k, int p, pid_t* children_pids)
{
    int active_children = n;
    while(active_children > 0)
    {   
        printf("[%d] Parent is sleeping for %d seconds\n", getpid(), k);
        sleep(k);
        for (int i = 0; i < n; i++)
        {
            kill(children_pids[i], SIGUSR1);
        }
        printf("[%d] Parent sent SIGUSR1 to all children\n", getpid());

        pid_t wpid;
        while ((wpid = waitpid(-1, NULL, WNOHANG)) > 0)
        {
            active_children--;
            printf("[%d] Child %d has finished. Active children left: %d\n", getpid(), wpid, active_children);
        }
        if (active_children == 0)
            break;

        printf("[%d] Parent is sleeping for %d seconds\n", getpid(), p);
        sleep(p);
        for (int i = 0; i < n; i++)
        {
            kill(children_pids[i], SIGUSR2);
        }
        printf("[%d] Parent sent SIGUSR2 to all children\n", getpid());
        while ((wpid = waitpid(-1, NULL, WNOHANG)) > 0)
        {
            active_children--;
            printf("[%d] Child %d has finished. Active children left: %d\n", getpid(), wpid, active_children);
        }
        if (active_children == 0)
            break;

        printf("[%d] Parent cycle finished\n", getpid());
    }
}

void create_children(int n, pid_t* children_pids, int r, int k, int p)
{
    for (int i = 0; i < n; i++)
    {
        pid_t child_pid = fork();
        switch(child_pid)
        {
            case -1:
                ERR("fork");
            case 0:
                set_handler(sigusr1_handler, SIGUSR1);
                set_handler(sigusr2_handler, SIGUSR2);
                child_work(r);
                exit(EXIT_SUCCESS);
            default:
                children_pids[i] = child_pid;
        }
        
    }
}



int main(int argc, char** argv)
{
    if (argc != 5)
        usage(argv[0]);
    
    if (atoi(argv[1]) <= 0 || atoi(argv[2]) <= 0 || atoi(argv[3]) <= 0 || atoi(argv[4]) <= 0)
        usage(argv[0]);

    int n = atoi(argv[1]);
    int k = atoi(argv[2]);
    int p = atoi(argv[3]);
    int r = atoi(argv[4]);

    pid_t* children_pids = malloc(n * sizeof(pid_t));
    if (children_pids == NULL)
        ERR("malloc");

    create_children(n, children_pids, r, k, p);
    parent_work(n, k, p, children_pids);
    exit(EXIT_SUCCESS);
}